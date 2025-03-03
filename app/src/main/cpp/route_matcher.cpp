/*
 * File: route_matcher.cpp
 * Description: Implementation of the RouteMatcher class, responsible for matching locations to road segments and generating navigation instructions.
 * Author: Giuseppe Franco
 * Created: March 2025
 */

#include "route_matcher.h"
#include <android/log.h>
#include <limits>
#include <cmath>
#include <algorithm>

#define LOG_TAG "RouteMatcher"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

constexpr double MAX_DISTANCE_TO_SEGMENT = 50.0;
constexpr double BEARING_WEIGHT = 0.5;
constexpr double DISTANCE_WEIGHT = 1.0;
constexpr double SEGMENT_SEARCH_RADIUS = 100.0;

RouteMatcher::RouteMatcher(RoadGraph* graph)
        : roadGraph(graph) {
    LOGI("RouteMatcher created");
}

RouteMatch RouteMatcher::match(const Location& loc) {
    LOGD("Matching location: %.6f, %.6f", loc.latitude, loc.longitude);

    lastLocation = loc;

    if (!currentRoute) {
        RouteMatch match;
        match.streetName = "No active route";
        match.nextManeuver = "Set a destination";
        match.distanceToNext = 0;
        match.estimatedTimeOfArrival = "";
        match.matchedLatitude = loc.latitude;
        match.matchedLongitude = loc.longitude;
        match.matchedBearing = loc.bearing;
        return match;
    }

    int closestPointIndex = findClosestPointOnRoute(loc);
    if (closestPointIndex < 0) {
        LOGE("Failed to find closest point on route");

        RouteMatch match;
        match.streetName = "Route matching error";
        match.nextManeuver = "Please recalculate route";
        match.distanceToNext = 0;
        match.estimatedTimeOfArrival = "";
        match.matchedLatitude = loc.latitude;
        match.matchedLongitude = loc.longitude;
        match.matchedBearing = loc.bearing;
        return match;
    }

    std::vector<RoadSegment*> nearbyRoads = roadGraph->findNearbyRoads(loc, SEGMENT_SEARCH_RADIUS);
    LOGD("Found %zu nearby road segments", nearbyRoads.size());

    if (nearbyRoads.empty()) {
        LOGD("No roads within %f meters, increasing search radius", SEGMENT_SEARCH_RADIUS);
        nearbyRoads = roadGraph->findNearbyRoads(loc, SEGMENT_SEARCH_RADIUS * 3);
        LOGD("Found %zu road segments with expanded search", nearbyRoads.size());
    }

    RoadSegment* bestSegment = nullptr;
    double bestScore = std::numeric_limits<double>::max();
    Location matchedLocation = loc;

    std::vector<RoadSegment*> routeSegments;
    for (RoadSegment* segment : nearbyRoads) {

        if (isSegmentOnRoute(segment)) {
            routeSegments.push_back(segment);
        }
    }

    const std::vector<RoadSegment*>& segmentsToCheck =
            routeSegments.empty() ? nearbyRoads : routeSegments;

    for (RoadSegment* segment : segmentsToCheck) {
        double score = calculateMatchScore(segment, loc);
        if (score < bestScore) {
            bestScore = score;
            bestSegment = segment;
            matchedLocation = projectOntoSegment(loc, *segment);
        }
    }

    LOGD("Map matching score: %f, matched to segment: %s",
         bestScore, bestSegment ? bestSegment->name.c_str() : "none");

    return createRouteMatch(matchedLocation, bestSegment, closestPointIndex);
}

bool RouteMatcher::isSegmentOnRoute(RoadSegment* segment) {
    if (!currentRoute || !segment) {
        return false;
    }

    double startLat = segment->start->latitude;
    double startLon = segment->start->longitude;
    double endLat = segment->end->latitude;
    double endLon = segment->end->longitude;

    const auto& points = currentRoute->points;

    for (size_t i = 0; i < points.size() - 1; i++) {
        double routeSegStartLat = points[i].latitude;
        double routeSegStartLon = points[i].longitude;
        double routeSegEndLat = points[i + 1].latitude;
        double routeSegEndLon = points[i + 1].longitude;

        double startToRouteStart = roadGraph->haversineDistance(
                startLat, startLon, routeSegStartLat, routeSegStartLon);
        double startToRouteEnd = roadGraph->haversineDistance(
                startLat, startLon, routeSegEndLat, routeSegEndLon);
        double endToRouteStart = roadGraph->haversineDistance(
                endLat, endLon, routeSegStartLat, routeSegStartLon);
        double endToRouteEnd = roadGraph->haversineDistance(
                endLat, endLon, routeSegEndLat, routeSegEndLon);

        const double MATCH_THRESHOLD = 20.0;
        if (startToRouteStart < MATCH_THRESHOLD || startToRouteEnd < MATCH_THRESHOLD ||
            endToRouteStart < MATCH_THRESHOLD || endToRouteEnd < MATCH_THRESHOLD) {
            return true;
        }

        double distance = calculateSegmentToSegmentDistance(
                startLat, startLon, endLat, endLon,
                routeSegStartLat, routeSegStartLon, routeSegEndLat, routeSegEndLon);

        if (distance < MATCH_THRESHOLD) {
            return true;
        }
    }

    return false;
}

void RouteMatcher::setRoute(const Route& route) {
    LOGI("Setting route with %zu points", route.points.size());
    currentRoute = route;

    validateRouteIntegrity();

    cumulativeDistances.clear();
    if (route.points.empty()) return;

    cumulativeDistances.push_back(0.0);

    for (size_t i = 1; i < route.points.size(); i++) {
        const auto& p1 = route.points[i-1];
        const auto& p2 = route.points[i];

        double segmentDist = roadGraph->haversineDistance(
                p1.latitude, p1.longitude,
                p2.latitude, p2.longitude
        );

        cumulativeDistances.push_back(cumulativeDistances.back() + segmentDist);
    }

    LOGI("Route total distance: %.1f meters", cumulativeDistances.back());

    precalculateRouteSegments();
}

void RouteMatcher::validateRouteIntegrity() {
    if (!currentRoute || currentRoute->points.size() < 2) return;

    const auto& points = currentRoute->points;
    const double MAX_GAP = 50.0;

    for (size_t i = 1; i < points.size(); i++) {
        const auto& p1 = points[i-1];
        const auto& p2 = points[i];

        double dist = roadGraph->haversineDistance(
                p1.latitude, p1.longitude,
                p2.latitude, p2.longitude
        );

        if (dist > MAX_GAP) {
            LOGW("Large gap detected in route at point %zu: %.1f meters", i, dist);

        }
    }
}

void RouteMatcher::precalculateRouteSegments() {
    routeSegments.clear();

    if (!currentRoute || currentRoute->points.size() < 2) {
        return;
    }

    const auto& points = currentRoute->points;

    for (size_t i = 0; i < points.size() - 1; i++) {
        const auto& p1 = points[i];
        const auto& p2 = points[i + 1];

        double midLat = (p1.latitude + p2.latitude) / 2.0;
        double midLon = (p1.longitude + p2.longitude) / 2.0;

        Location midpoint{midLat, midLon, 0.0f, 0.0f};

        std::vector<RoadSegment*> nearbyRoads = roadGraph->findNearbyRoads(midpoint, 50.0);

        if (nearbyRoads.empty()) {
            nearbyRoads = roadGraph->findNearbyRoads(midpoint, 100.0);
        }

        double routeBearing = calculateBearing(
                p1.latitude, p1.longitude,
                p2.latitude, p2.longitude
        );

        RoadSegment* bestSegment = nullptr;
        double bestScore = std::numeric_limits<double>::max();

        for (RoadSegment* segment : nearbyRoads) {

            double segmentBearing = calculateBearing(
                    segment->start->latitude, segment->start->longitude,
                    segment->end->latitude, segment->end->longitude
            );

            double bearingDiff = std::abs(segmentBearing - routeBearing);
            if (bearingDiff > 180.0) {
                bearingDiff = 360.0 - bearingDiff;
            }

            Location projected = projectOntoSegment(midpoint, *segment);
            double distance = roadGraph->haversineDistance(
                    midpoint.latitude, midpoint.longitude,
                    projected.latitude, projected.longitude
            );

            double score = distance + (bearingDiff / 45.0) * 20.0;

            if (score < bestScore) {
                bestScore = score;
                bestSegment = segment;
            }
        }

        if (bestSegment) {
            routeSegments.push_back(bestSegment);
            LOGD("Route segment %zu matched to road: %s", i, bestSegment->name.c_str());
        } else {
            LOGD("No matching road segment found for route segment %zu", i);
        }
    }

    LOGI("Precalculated %zu road segments for route", routeSegments.size());
}

int RouteMatcher::findClosestPointOnRoute(const Location& loc) {
    if (!currentRoute || currentRoute->points.empty()) {
        return -1;
    }

    const auto& points = currentRoute->points;

    int closestIdx = -1;
    double closestDist = std::numeric_limits<double>::max();

    for (size_t i = 0; i < points.size(); i++) {
        const auto& point = points[i];
        double dist = roadGraph->haversineDistance(
                loc.latitude, loc.longitude,
                point.latitude, point.longitude
        );

        if (dist < closestDist) {
            closestDist = dist;
            closestIdx = static_cast<int>(i);
        }
    }

    if (closestIdx < static_cast<int>(points.size() - 1)) {
        const auto& point = points[closestIdx];
        const auto& nextPoint = points[closestIdx + 1];

        double distToNext = roadGraph->haversineDistance(
                loc.latitude, loc.longitude,
                nextPoint.latitude, nextPoint.longitude
        );

        double segmentLength = roadGraph->haversineDistance(
                point.latitude, point.longitude,
                nextPoint.latitude, nextPoint.longitude
        );

        double progress = 1.0 - (distToNext / segmentLength);

        if (progress > 0.7) {
            double bearingToNext = calculateBearing(
                    loc.latitude, loc.longitude,
                    nextPoint.latitude, nextPoint.longitude
            );

            double bearingDiff = std::abs(bearingToNext - loc.bearing);
            if (bearingDiff > 180.0) {
                bearingDiff = 360.0 - bearingDiff;
            }

            if (bearingDiff < 45.0) {
                closestIdx++;
            }
        }
    }

    return closestIdx;
}

double RouteMatcher::calculateMatchScore(const RoadSegment* segment, const Location& loc) {
    if (!segment) return std::numeric_limits<double>::max();

    Location projected = projectOntoSegment(loc, *segment);

    double perpDistance = roadGraph->haversineDistance(
            loc.latitude, loc.longitude,
            projected.latitude, projected.longitude
    );

    if (perpDistance > MAX_DISTANCE_TO_SEGMENT) {
        return std::numeric_limits<double>::max();
    }

    double segmentBearing = calculateBearing(
            segment->start->latitude, segment->start->longitude,
            segment->end->latitude, segment->end->longitude
    );

    double bearingDiff = std::abs(segmentBearing - loc.bearing);
    if (bearingDiff > 180.0) {
        bearingDiff = 360.0 - bearingDiff;
    }

    double bearingFactor = bearingDiff / 180.0;

    bool isOnRoute = false;
    if (currentRoute) {
        isOnRoute = std::find(routeSegments.begin(), routeSegments.end(), segment) != routeSegments.end();
    }

    double routeBonus = isOnRoute ? 0.5 : 1.0;

    double speedFactor = 1.0;
    if (loc.speed > 1.0f) {

        if (segment->speedLimit > 60.0) {
            speedFactor = 0.8;
        } else if (segment->speedLimit < 30.0 && loc.speed > 10.0f) {
            speedFactor = 1.2;
        }
    } else if (loc.speed < 5.0f && segment->speedLimit > 70.0) {
        speedFactor = 1.2;
    }

    double score = (DISTANCE_WEIGHT * perpDistance +
                    BEARING_WEIGHT * bearingFactor * 50.0) *
                   routeBonus * speedFactor;
    return score;
}

double RouteMatcher::calculateSegmentToSegmentDistance(
        double lat1a, double lon1a, double lat1b, double lon1b,
        double lat2a, double lon2a, double lat2b, double lon2b) {

    Location loc1a{lat1a, lon1a, 0.0f, 0.0f};
    Location loc1b{lat1b, lon1b, 0.0f, 0.0f};

    RoadSegment segment;
    segment.start = new Node();
    segment.end = new Node();
    segment.start->latitude = lat2a;
    segment.start->longitude = lon2a;
    segment.end->latitude = lat2b;
    segment.end->longitude = lon2b;

    Location proj1a = projectOntoSegment(loc1a, segment);
    Location proj1b = projectOntoSegment(loc1b, segment);

    double dist1a = roadGraph->haversineDistance(
            lat1a, lon1a, proj1a.latitude, proj1a.longitude);
    double dist1b = roadGraph->haversineDistance(
            lat1b, lon1b, proj1b.latitude, proj1b.longitude);

    delete segment.start;
    delete segment.end;

    return std::min(dist1a, dist1b);
}

Location RouteMatcher::projectOntoSegment(const Location& loc, const RoadSegment& segment) {

    double x1 = segment.start->longitude;
    double y1 = segment.start->latitude;
    double x2 = segment.end->longitude;
    double y2 = segment.end->latitude;
    double x0 = loc.longitude;
    double y0 = loc.latitude;

    double dx = x2 - x1;
    double dy = y2 - y1;
    double segmentLengthSquared = dx*dx + dy*dy;

    if (segmentLengthSquared < 1e-10) {
        return Location{y1, x1, 0, 0};
    }

    double t = ((x0 - x1) * dx + (y0 - y1) * dy) / segmentLengthSquared;

    t = std::max(0.0, std::min(1.0, t));

    double projX = x1 + t * dx;
    double projY = y1 + t * dy;

    double segmentBearing = calculateBearing(y1, x1, y2, x2);

    float projSpeed = loc.speed;

    if (loc.speed > 0.5f) {

        double locBearingRad = loc.bearing * M_PI / 180.0;
        double vx = std::sin(locBearingRad);
        double vy = std::cos(locBearingRad);
        double segmentDotProduct = dx * vx + dy * vy;

        if (segmentDotProduct < 0) {
            segmentBearing = std::fmod(segmentBearing + 180.0, 360.0);
        }
    }

    return Location{projY, projX, static_cast<float>(segmentBearing), projSpeed};
}

RouteMatch RouteMatcher::createRouteMatch(
        const Location& matched,
        const RoadSegment* segment,
        int closestPointIndex) {

    RouteMatch match;

    match.matchedLatitude = matched.latitude;
    match.matchedLongitude = matched.longitude;
    match.matchedBearing = matched.bearing;

    match.streetName = segment ? segment->name : "Unknown Road";

    int distanceToNext = 0;
    if (currentRoute && closestPointIndex >= 0) {
        const auto& points = currentRoute->points;

        int nextManeuverIndex = findNextManeuverPoint(closestPointIndex);

        if (nextManeuverIndex > closestPointIndex) {

            distanceToNext = static_cast<int>(
                    cumulativeDistances[nextManeuverIndex] - cumulativeDistances[closestPointIndex]
            );

            match.nextManeuver = determineNextManeuver(closestPointIndex, nextManeuverIndex);
        } else {

            match.nextManeuver = "Arrive at destination";

            if (closestPointIndex < static_cast<int>(points.size() - 1)) {
                distanceToNext = static_cast<int>(
                        cumulativeDistances.back() - cumulativeDistances[closestPointIndex]
                );
            } else {
                distanceToNext = 0;
            }
        }
    } else {
        match.nextManeuver = "Follow route";
    }

    match.distanceToNext = distanceToNext;

    match.estimatedTimeOfArrival = "";

    return match;
}

int RouteMatcher::findNextManeuverPoint(int currentIndex) {
    if (!currentRoute || currentRoute->points.empty() ||
        currentIndex < 0 || currentIndex >= static_cast<int>(currentRoute->points.size())) {
        return -1;
    }

    const auto& points = currentRoute->points;

    for (int i = currentIndex + 1; i < static_cast<int>(points.size()) - 1; i++) {

        double bearing1 = calculateBearing(
                points[i-1].latitude, points[i-1].longitude,
                points[i].latitude, points[i].longitude
        );

        double bearing2 = calculateBearing(
                points[i].latitude, points[i].longitude,
                points[i+1].latitude, points[i+1].longitude
        );

        double bearingDiff = std::abs(bearing1 - bearing2);
        if (bearingDiff > 180.0) {
            bearingDiff = 360.0 - bearingDiff;
        }

        if (bearingDiff > 30.0) {
            return i;
        }
    }

    return static_cast<int>(points.size() - 1);
}

std::string RouteMatcher::determineNextManeuver(int currentIndex, int maneuverIndex) {
    if (!currentRoute || currentRoute->points.empty() ||
        currentIndex < 0 || maneuverIndex <= currentIndex ||
        maneuverIndex >= static_cast<int>(currentRoute->points.size())) {
        return "Follow route";
    }

    const auto& points = currentRoute->points;

    double currentBearing = calculateBearing(
            points[maneuverIndex-1].latitude, points[maneuverIndex-1].longitude,
            points[maneuverIndex].latitude, points[maneuverIndex].longitude
    );

    double nextBearing = calculateBearing(
            points[maneuverIndex].latitude, points[maneuverIndex].longitude,
            points[maneuverIndex+1 < static_cast<int>(points.size()) ? maneuverIndex+1 : maneuverIndex].latitude,
            points[maneuverIndex+1 < static_cast<int>(points.size()) ? maneuverIndex+1 : maneuverIndex].longitude
    );

    double angle = nextBearing - currentBearing;

    while (angle > 180.0) angle -= 360.0;
    while (angle < -180.0) angle += 360.0;

    if (std::abs(angle) < 20.0) {
        return "Continue straight";
    } else if (angle >= 20.0 && angle < 60.0) {
        return "Turn slight right";
    } else if (angle >= 60.0 && angle < 120.0) {
        return "Turn right";
    } else if (angle >= 120.0) {
        return "Make a sharp right";
    } else if (angle <= -20.0 && angle > -60.0) {
        return "Turn slight left";
    } else if (angle <= -60.0 && angle > -120.0) {
        return "Turn left";
    } else if (angle <= -120.0) {
        return "Make a sharp left";
    }

    return "Follow route";
}

double RouteMatcher::calculateBearing(double lat1, double lon1, double lat2, double lon2) {

    lat1 *= M_PI / 180.0;
    lon1 *= M_PI / 180.0;
    lat2 *= M_PI / 180.0;
    lon2 *= M_PI / 180.0;

    double dLon = lon2 - lon1;
    double y = std::sin(dLon) * std::cos(lat2);
    double x = std::cos(lat1) * std::sin(lat2) - std::sin(lat1) * std::cos(lat2) * std::cos(dLon);
    double bearing = std::atan2(y, x) * 180.0 / M_PI;

    bearing = std::fmod(bearing + 360.0, 360.0);

    return bearing;
}
