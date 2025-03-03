/*
 * File: routing_engine.cpp
 * Description: Implementation of the RoutingEngine class, responsible for calculating routes, generating alternatives, and managing pathfinding logic.
 * Author: Giuseppe Franco
 * Created: March 2025
 */

#include "routing_engine.h"
#include <android/log.h>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <random>
#include <cmath>
#include <functional>
#include <algorithm>

#define LOG_TAG "RoutingEngine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

constexpr double MAX_ROUTE_DISTANCE = 10000.0;
constexpr double NODE_SEARCH_RADIUS = 10000.0;
constexpr int MAX_ROUTE_POINTS = 1000;
constexpr double ROUTE_POINT_SPACING = 25.0;

RoutingEngine::RoutingEngine(RoadGraph* graph)
        : roadGraph(graph) {
    LOGI("RoutingEngine created");
}

std::vector<Route> RoutingEngine::calculateRoutes(const Location& start, const Location& end) {
    LOGI("Calculating route from (%.6f, %.6f) to (%.6f, %.6f)",
         start.latitude, start.longitude, end.latitude, end.longitude);

    double directDistance = roadGraph->haversineDistance(
            start.latitude, start.longitude,
            end.latitude, end.longitude
    );

    LOGI("Direct distance between points: %.1f meters", directDistance);

    if (directDistance > MAX_ROUTE_DISTANCE) {
        LOGE("Distance exceeds maximum supported route distance (%.1f > %.1f)",
             directDistance, MAX_ROUTE_DISTANCE);

        Route directRoute = createDirectRoute(start, end);
        return {directRoute};
    }

    Node* startNode = findNearestNode(start, NODE_SEARCH_RADIUS);
    Node* endNode = findNearestNode(end, NODE_SEARCH_RADIUS);

    if (!startNode || !endNode) {
        LOGE("Failed to find start or end node (null). Falling back to direct route.");
        Route directRoute = createDirectRoute(start, end);
        return {directRoute};
    }

    std::vector<Node*> primaryPath = findPath(startNode, endNode);
    if (primaryPath.empty()) {
        LOGE("Failed to find path via A*, falling back to direct route.");
        Route directRoute = createDirectRoute(start, end);
        return {directRoute};
    }

    Route primaryRoute = createDetailedRoute(primaryPath, generateRouteId(), start, end);

    std::vector<Route> routes;
    routes.push_back(primaryRoute);

    auto altRoutes = generateAlternatives(primaryRoute, start, end);
    routes.insert(routes.end(), altRoutes.begin(), altRoutes.end());

    LOGI("Generated %zu routes", routes.size());
    return routes;
}

Route RoutingEngine::createDirectRoute(const Location& start, const Location& end) {
    LOGI("Creating direct route with intermediate points");

    Route route;
    route.id = generateRouteId();
    route.name = "Direct Route";

    double distance = roadGraph->haversineDistance(
            start.latitude, start.longitude,
            end.latitude, end.longitude
    );

    int numPoints = std::min(
            MAX_ROUTE_POINTS,
            std::max(20, static_cast<int>(distance / ROUTE_POINT_SPACING))
    );

    route.points.push_back(start);

    for (int i = 1; i < numPoints - 1; i++) {
        double fraction = static_cast<double>(i) / (numPoints - 1);
        double lat = start.latitude + fraction * (end.latitude - start.latitude);
        double lon = start.longitude + fraction * (end.longitude - start.longitude);

        double bearing = calculateBearing(
                route.points.back().latitude, route.points.back().longitude,
                lat, lon
        );

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> small_var(-0.000005, 0.000005);

        route.points.push_back(Location{
                lat + small_var(gen),
                lon + small_var(gen),
                static_cast<float>(bearing),
                10.0f
        });
    }

    route.points.push_back(end);

    route.durationSeconds = calculateRouteDuration(route);

    return route;
}

Route RoutingEngine::createDetailedRoute(const std::vector<Node*>& path,
                                         const std::string& id,
                                         const Location& start,
                                         const Location& end) {
    Route route;
    route.id = id;
    route.name = "Route to Destination";

    std::vector<Location> allPoints;

    allPoints.push_back(start);

    if (!path.empty()) {

        double startToFirstDist = roadGraph->haversineDistance(
                start.latitude, start.longitude,
                path[0]->latitude, path[0]->longitude
        );

        if (startToFirstDist > 10.0) {

            addIntermediatePoints(allPoints, start,
                                  Location(path[0]->latitude, path[0]->longitude, 0, 0),
                                  3);
        }
    }

    for (size_t i = 0; i < path.size(); i++) {
        Node* current = path[i];

        if (i < path.size() - 1) {
            Node* next = path[i+1];

            RoadSegment* segment = findConnectingSegment(current, next);
            if (segment) {

                allPoints.push_back(Location(current->latitude, current->longitude, 0, 0));
            } else {

                Location currentLoc(current->latitude, current->longitude, 0, 0);
                Location nextLoc(next->latitude, next->longitude, 0, 0);

                allPoints.push_back(currentLoc);

                double nodeDist = roadGraph->haversineDistance(
                        current->latitude, current->longitude,
                        next->latitude, next->longitude
                );

                int pointCount = std::max(2, static_cast<int>(nodeDist / 20.0));
                addIntermediatePoints(allPoints, currentLoc, nextLoc, pointCount);
            }
        } else {

            allPoints.push_back(Location(current->latitude, current->longitude, 0, 0));
        }
    }

    if (!path.empty()) {
        Node* lastNode = path.back();
        double lastToEndDist = roadGraph->haversineDistance(
                lastNode->latitude, lastNode->longitude,
                end.latitude, end.longitude
        );

        if (lastToEndDist > 10.0) {

            Location lastLoc(lastNode->latitude, lastNode->longitude, 0, 0);
            addIntermediatePoints(allPoints, lastLoc, end, 3);
        }
    }

    allPoints.push_back(end);

    calculateBearingAndSpeed(allPoints);

    route.points = allPoints;

    route.durationSeconds = calculateRouteDuration(route);

    smoothRoutePath(route);

    return route;
}

void RoutingEngine::addIntermediatePoints(std::vector<Location>& points,
                                          const Location& start,
                                          const Location& end,
                                          int count) {
    for (int i = 1; i <= count; i++) {
        double ratio = static_cast<double>(i) / (count + 1);

        double lat = start.latitude + ratio * (end.latitude - start.latitude);
        double lon = start.longitude + ratio * (end.longitude - start.longitude);

        points.push_back(Location(lat, lon, 0, 0));
    }
}

RoadSegment* RoutingEngine::findConnectingSegment(Node* from, Node* to) {

    for (RoadSegment* segment : from->segments) {
        if (segment->end == to) {
            return segment;
        }
    }

    return nullptr;
}

void RoutingEngine::smoothRoutePath(Route& route) {
    if (route.points.size() < 3) {
        return;
    }

    std::vector<Location> simplified;
    simplified.push_back(route.points.front());

    const double MIN_ANGLE_CHANGE = 20.0;

    for (size_t i = 1; i < route.points.size() - 1; i++) {
        const Location& prev = simplified.back();
        const Location& curr = route.points[i];
        const Location& next = route.points[i + 1];

        double bearing1 = calculateBearing(
                prev.latitude, prev.longitude,
                curr.latitude, curr.longitude
        );

        double bearing2 = calculateBearing(
                curr.latitude, curr.longitude,
                next.latitude, next.longitude
        );

        double diff = std::abs(bearing1 - bearing2);
        if (diff > 180.0) {
            diff = 360.0 - diff;
        }

        if (diff > MIN_ANGLE_CHANGE) {
            simplified.push_back(curr);
        }

        else if (roadGraph->haversineDistance(
                prev.latitude, prev.longitude,
                curr.latitude, curr.longitude) > 50.0) {
            simplified.push_back(curr);
        }
    }

    simplified.push_back(route.points.back());

    if (simplified.size() > 3) {
        std::vector<Location> filtered;
        filtered.push_back(simplified.front());

        for (size_t i = 1; i < simplified.size() - 1; i++) {

            const Location& prev = filtered.back();
            const Location& curr = simplified[i];
            const Location& next = simplified[i + 1];

            double prevToCurrDist = roadGraph->haversineDistance(
                    prev.latitude, prev.longitude,
                    curr.latitude, curr.longitude
            );

            double currToNextDist = roadGraph->haversineDistance(
                    curr.latitude, curr.longitude,
                    next.latitude, next.longitude
            );

            double prevToNextDist = roadGraph->haversineDistance(
                    prev.latitude, prev.longitude,
                    next.latitude, next.longitude
            );

            if (prevToNextDist < (prevToCurrDist + currToNextDist) * 0.8) {
                continue;
            }

            filtered.push_back(curr);
        }

        filtered.push_back(simplified.back());
        simplified = filtered;
    }

    route.points = simplified;

    calculateBearingAndSpeed(route.points);
}

std::vector<Location> RoutingEngine::douglasPeucker(const std::vector<Location>& points, double epsilon) {
    if (points.size() <= 2) {
        return points;
    }

    double dmax = 0;
    size_t index = 0;

    for (size_t i = 1; i < points.size() - 1; i++) {
        double d = perpendicularDistance(points[i], points[0], points[points.size() - 1]);
        if (d > dmax) {
            index = i;
            dmax = d;
        }
    }

    std::vector<Location> result;
    if (dmax > epsilon) {

        auto firstPart = douglasPeucker(
                std::vector<Location>(points.begin(), points.begin() + index + 1),
                epsilon
        );
        auto secondPart = douglasPeucker(
                std::vector<Location>(points.begin() + index, points.end()),
                epsilon
        );

        result.insert(result.end(), firstPart.begin(), firstPart.end() - 1);
        result.insert(result.end(), secondPart.begin(), secondPart.end());
    } else {

        result.push_back(points.front());
        result.push_back(points.back());
    }

    return result;
}

double RoutingEngine::perpendicularDistance(const Location& point, const Location& lineStart, const Location& lineEnd) {
    double x = point.longitude;
    double y = point.latitude;
    double x1 = lineStart.longitude;
    double y1 = lineStart.latitude;
    double x2 = lineEnd.longitude;
    double y2 = lineEnd.latitude;

    double A = y2 - y1;
    double B = x1 - x2;
    double C = x2 * y1 - x1 * y2;

    double dist = std::abs(A * x + B * y + C) / std::sqrt(A * A + B * B);

    double metersPerDegree = 111000.0;
    return dist * metersPerDegree;
}

std::vector<Node*> RoutingEngine::findPath(Node* start, Node* end) {

    if (start == end) {
        LOGI("A* findPath: start node == end node => single-node path");
        return {start};
    }

    std::priority_queue<NodeData, std::vector<NodeData>, std::greater<NodeData>> openSet;
    std::unordered_set<Node*> closedSet;
    std::unordered_map<Node*, Node*> cameFrom;
    std::unordered_map<Node*, double> gScore;

    openSet.push({ start, 0.0 });
    gScore[start] = 0.0;

    while (!openSet.empty()) {
        NodeData current = openSet.top();
        openSet.pop();

        if (current.node == end) {

            std::vector<Node*> path;
            Node* node = end;
            while (node != start) {
                path.push_back(node);
                node = cameFrom[node];
            }
            path.push_back(start);
            std::reverse(path.begin(), path.end());
            return path;
        }

        if (closedSet.find(current.node) != closedSet.end()) {
            continue;
        }
        closedSet.insert(current.node);

        for (RoadSegment* segment : current.node->segments) {
            Node* neighbor = segment->end;
            if (closedSet.find(neighbor) != closedSet.end()) {
                continue;
            }
            double tentativeG = gScore[current.node] + segment->length;
            if (gScore.find(neighbor) == gScore.end() || tentativeG < gScore[neighbor]) {
                cameFrom[neighbor] = current.node;
                gScore[neighbor]   = tentativeG;
                double heuristic   = estimateHeuristic(neighbor, end);
                openSet.push({ neighbor, tentativeG + heuristic });
            }
        }
    }

    return {};
}

double RoutingEngine::estimateHeuristic(const Node* current, const Node* goal) {

    return roadGraph->haversineDistance(
            current->latitude, current->longitude,
            goal->latitude,    goal->longitude
    );
}

std::string RoutingEngine::generateRouteId() {

    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char* digits = "0123456789abcdef";

    std::string uuid = "route-";
    for (int i = 0; i < 8; ++i) {
        uuid += digits[dis(gen)];
    }
    return uuid;
}

Node* RoutingEngine::findNearestNode(const Location& location, double searchRadius) {
    LOGD("findNearestNode: location=(%.6f, %.6f). Searching up to %.1f meters.",
         location.latitude, location.longitude, searchRadius);

    std::vector<RoadSegment*> nearbyRoads = roadGraph->findNearbyRoads(location, searchRadius);

    LOGD("findNearestNode: found %zu roads within %.1f meters", nearbyRoads.size(), searchRadius);

    if (nearbyRoads.empty()) {
        LOGE("No roads found near (%.6f, %.6f)", location.latitude, location.longitude);
        return nullptr;
    }

    Node* nearest = nullptr;
    double minDistance = std::numeric_limits<double>::max();

    for (RoadSegment* segment : nearbyRoads) {

        double distToStart = roadGraph->haversineDistance(
                location.latitude, location.longitude,
                segment->start->latitude, segment->start->longitude
        );

        if (distToStart < minDistance) {
            minDistance = distToStart;
            nearest = segment->start;
        }

        double distToEnd = roadGraph->haversineDistance(
                location.latitude, location.longitude,
                segment->end->latitude, segment->end->longitude
        );

        if (distToEnd < minDistance) {
            minDistance = distToEnd;
            nearest = segment->end;
        }
    }

    for (RoadSegment* segment : nearbyRoads) {

        Location loc{location.latitude, location.longitude, 0.0f, 0.0f};

        Location projected = projectLocationOntoSegment(loc, segment);

        double distance = roadGraph->haversineDistance(
                location.latitude, location.longitude,
                projected.latitude, projected.longitude
        );

        if (distance < minDistance) {

            double distToStart = roadGraph->haversineDistance(
                    projected.latitude, projected.longitude,
                    segment->start->latitude, segment->start->longitude
            );

            double distToEnd = roadGraph->haversineDistance(
                    projected.latitude, projected.longitude,
                    segment->end->latitude, segment->end->longitude
            );

            const double MIN_NODE_SPACING = 10.0;

            if (distToStart > MIN_NODE_SPACING && distToEnd > MIN_NODE_SPACING) {

                std::string nodeId = "projected_" + std::to_string(segment->id) + "_" +
                                     std::to_string(static_cast<int>(projected.latitude * 1000000)) + "_" +
                                     std::to_string(static_cast<int>(projected.longitude * 1000000));

                Node* newNode = roadGraph->addNode(nodeId, projected.latitude, projected.longitude);

                roadGraph->addSegment(segment->start, newNode, segment->name, segment->speedLimit, segment->type);
                roadGraph->addSegment(newNode, segment->end, segment->name, segment->speedLimit, segment->type);

                minDistance = distance;
                nearest = newNode;
            }
        }
    }

    if (nearest) {
        LOGD("findNearestNode => Nearest node is (%.6f, %.6f), dist=%.2f",
             nearest->latitude, nearest->longitude, minDistance);
    }

    return nearest;
}

Location RoutingEngine::projectLocationOntoSegment(const Location& loc, RoadSegment* segment) {

    double x1 = segment->start->longitude;
    double y1 = segment->start->latitude;
    double x2 = segment->end->longitude;
    double y2 = segment->end->latitude;
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

    double bearing = calculateBearing(y1, x1, y2, x2);

    return Location{projY, projX, static_cast<float>(bearing), 0};
}

std::vector<Route> RoutingEngine::generateAlternatives(const Route& primaryRoute,
                                                       const Location& start,
                                                       const Location& end) {

    std::vector<Route> alternatives;

    if (primaryRoute.points.size() < 2) {
        LOGI("Route has too few points to generate alternatives");
        return alternatives;
    }

    Node* startNode = findNearestNode(start);
    Node* endNode = findNearestNode(end);

    if (!startNode || !endNode) {
        LOGE("Failed to find start or end node for alternatives");
        return alternatives;
    }

    Route fastRoute = generateFastRoute(startNode, endNode, start, end);
    if (!fastRoute.points.empty() && isRouteDifferentEnough(fastRoute, primaryRoute)) {
        fastRoute.name = "Fastest Route";
        alternatives.push_back(fastRoute);
    }

    Route noHighwaysRoute = generateNoHighwaysRoute(startNode, endNode, start, end);
    if (!noHighwaysRoute.points.empty() && isRouteDifferentEnough(noHighwaysRoute, primaryRoute)) {
        noHighwaysRoute.name = "No Highways";
        alternatives.push_back(noHighwaysRoute);
    }

    LOGI("Generated %zu alternative routes", alternatives.size());
    return alternatives;
}

Route RoutingEngine::generateFastRoute(Node* start, Node* end,
                                       const Location& startLoc,
                                       const Location& endLoc) {
    LOGI("Generating fast route");

    auto speedCostFunction = [](RoadSegment* segment) -> double {

        double speedFactor = 50.0 / segment->speedLimit;
        return segment->length * speedFactor;
    };

    std::vector<Node*> path = findPathWithCostFunction(start, end, speedCostFunction);

    if (path.empty()) {
        LOGE("Failed to find fast route path");
        return Route();
    }

    Route route = createDetailedRoute(path, generateRouteId(), startLoc, endLoc);
    route.name = "Fastest Route";

    route.durationSeconds = calculateCustomDuration(route, 1.2);

    return route;
}

Route RoutingEngine::generateNoHighwaysRoute(Node* start, Node* end,
                                             const Location& startLoc,
                                             const Location& endLoc) {
    LOGI("Generating no-highways route");

    auto noHighwaysCostFunction = [](RoadSegment* segment) -> double {
        double baseCost = segment->length;

        if (segment->type == RoadType::HIGHWAY) {
            return baseCost * 10.0;
        }

        return baseCost;
    };

    std::vector<Node*> path = findPathWithCostFunction(start, end, noHighwaysCostFunction);

    if (path.empty()) {
        LOGE("Failed to find no-highways route path");
        return Route();
    }

    Route route = createDetailedRoute(path, generateRouteId(), startLoc, endLoc);
    route.name = "No Highways";

    route.durationSeconds = calculateCustomDuration(route, 0.8);

    return route;
}

std::vector<Node*> RoutingEngine::findPathWithCostFunction(
        Node* start, Node* end,
        std::function<double(RoadSegment*)> costFunction) {

    if (start == end) {
        return {start};
    }

    std::priority_queue<NodeData, std::vector<NodeData>, std::greater<NodeData>> openSet;
    std::unordered_set<Node*> closedSet;
    std::unordered_map<Node*, Node*> cameFrom;
    std::unordered_map<Node*, double> gScore;

    openSet.push({ start, 0.0 });
    gScore[start] = 0.0;

    while (!openSet.empty()) {
        NodeData current = openSet.top();
        openSet.pop();

        if (current.node == end) {

            std::vector<Node*> path;
            Node* node = end;
            while (node != start) {
                path.push_back(node);
                node = cameFrom[node];
            }
            path.push_back(start);
            std::reverse(path.begin(), path.end());
            return path;
        }

        if (closedSet.find(current.node) != closedSet.end()) {
            continue;
        }
        closedSet.insert(current.node);

        for (RoadSegment* segment : current.node->segments) {
            Node* neighbor = segment->end;
            if (closedSet.find(neighbor) != closedSet.end()) {
                continue;
            }

            double segmentCost = costFunction(segment);
            double tentativeG = gScore[current.node] + segmentCost;

            if (gScore.find(neighbor) == gScore.end() || tentativeG < gScore[neighbor]) {
                cameFrom[neighbor] = current.node;
                gScore[neighbor] = tentativeG;
                double heuristic = estimateHeuristic(neighbor, end);
                openSet.push({ neighbor, tentativeG + heuristic });
            }
        }
    }

    return {};
}

int RoutingEngine::calculateCustomDuration(const Route& route, double speedFactor) {

    double totalDistance = 0.0;
    for (size_t i = 1; i < route.points.size(); ++i) {
        const Location& p1 = route.points[i - 1];
        const Location& p2 = route.points[i];
        totalDistance += roadGraph->haversineDistance(p1.latitude, p1.longitude,
                                                      p2.latitude, p2.longitude);
    }

    double avgSpeed = 9.72 * speedFactor;
    int durationSec = static_cast<int>(totalDistance / avgSpeed);
    return durationSec;
}

bool RoutingEngine::isRouteDifferentEnough(const Route& route1, const Route& route2) {

    if (route1.points.size() < 2 || route2.points.size() < 2) {
        return false;
    }

    const Location& start1 = route1.points.front();
    const Location& end1 = route1.points.back();
    const Location& start2 = route2.points.front();
    const Location& end2 = route2.points.back();

    double startDist = roadGraph->haversineDistance(start1.latitude, start1.longitude,
                                                    start2.latitude, start2.longitude);
    double endDist = roadGraph->haversineDistance(end1.latitude, end1.longitude,
                                                  end2.latitude, end2.longitude);

    if (startDist > 100.0 || endDist > 100.0) {
        return false;
    }

    int sharedPoints = 0;
    int totalPoints = 0;

    const int sampleCount = 10;

    for (int i = 0; i < sampleCount; i++) {
        double t = static_cast<double>(i) / (sampleCount - 1);

        Location p1 = getRoutePointAtFraction(route1, t);
        Location p2 = getRoutePointAtFraction(route2, t);

        double dist = roadGraph->haversineDistance(p1.latitude, p1.longitude,
                                                   p2.latitude, p2.longitude);

        if (dist < 200.0) {
            sharedPoints++;
        }
        totalPoints++;
    }

    double similarity = static_cast<double>(sharedPoints) / totalPoints;

    return similarity < 0.7;
}

Location RoutingEngine::getRoutePointAtFraction(const Route& route, double fraction) {

    if (route.points.empty()) {
        return Location{0, 0, 0, 0};
    }

    if (route.points.size() == 1 || fraction <= 0.0) {
        return route.points.front();
    }

    if (fraction >= 1.0) {
        return route.points.back();
    }

    std::vector<double> cumDistances;
    cumDistances.push_back(0.0);

    for (size_t i = 1; i < route.points.size(); i++) {
        const Location& p1 = route.points[i-1];
        const Location& p2 = route.points[i];

        double segmentDist = roadGraph->haversineDistance(p1.latitude, p1.longitude,
                                                          p2.latitude, p2.longitude);
        cumDistances.push_back(cumDistances.back() + segmentDist);
    }

    double totalDistance = cumDistances.back();
    double targetDistance = totalDistance * fraction;

    for (size_t i = 1; i < cumDistances.size(); i++) {
        if (cumDistances[i] >= targetDistance) {

            const Location& p1 = route.points[i-1];
            const Location& p2 = route.points[i];

            double segmentDist = cumDistances[i] - cumDistances[i-1];
            double segmentFraction = (targetDistance - cumDistances[i-1]) / segmentDist;

            double lat = p1.latitude + segmentFraction * (p2.latitude - p1.latitude);
            double lon = p1.longitude + segmentFraction * (p2.longitude - p1.longitude);

            double bearing = p1.bearing + segmentFraction * (p2.bearing - p1.bearing);

            float speed = p1.speed + segmentFraction * (p2.speed - p1.speed);

            return Location{lat, lon, static_cast<float>(bearing), speed};
        }
    }

    return route.points.back();
}

int RoutingEngine::calculateRouteDuration(const Route& route) {

    double totalDistance = 0.0;
    double totalTime = 0.0;

    for (size_t i = 1; i < route.points.size(); ++i) {
        const Location& p1 = route.points[i - 1];
        const Location& p2 = route.points[i];

        double distance = roadGraph->haversineDistance(
                p1.latitude, p1.longitude,
                p2.latitude, p2.longitude
        );

        totalDistance += distance;

        if (p1.speed > 0.1f) {

            double segmentTime = distance / p1.speed;
            totalTime += segmentTime;
        }
    }

    if (totalTime > 0.0) {
        return static_cast<int>(totalTime);
    }

    double avgSpeed = 9.72;
    return static_cast<int>(totalDistance / avgSpeed);
}

double RoutingEngine::calculateBearing(double lat1, double lon1, double lat2, double lon2) {

    lat1 *= M_PI / 180.0;
    lon1 *= M_PI / 180.0;
    lat2 *= M_PI / 180.0;
    lon2 *= M_PI / 180.0;

    double dLon = lon2 - lon1;
    double y = std::sin(dLon) * std::cos(lat2);
    double x = std::cos(lat1) * std::sin(lat2) -
               std::sin(lat1) * std::cos(lat2) * std::cos(dLon);
    double bearing = std::atan2(y, x) * 180.0 / M_PI;

    bearing = std::fmod(bearing + 360.0, 360.0);

    return bearing;
}

void RoutingEngine::calculateBearingAndSpeed(std::vector<Location>& path) {
    if (path.size() < 2) return;

    for (size_t i = 0; i < path.size() - 1; i++) {
        double dLon = (path[i + 1].longitude - path[i].longitude) * M_PI / 180.0;
        double lat1 = path[i].latitude * M_PI / 180.0;
        double lat2 = path[i + 1].latitude * M_PI / 180.0;

        double y = std::sin(dLon) * std::cos(lat2);
        double x = std::cos(lat1) * std::sin(lat2)
                   - std::sin(lat1) * std::cos(lat2) * std::cos(dLon);

        float bearing = static_cast<float>(std::atan2(y, x) * 180.0 / M_PI);
        if (bearing < 0.0f) bearing += 360.0f;
        path[i].bearing = bearing;

        double distance = roadGraph->haversineDistance(
                path[i].latitude, path[i].longitude,
                path[i + 1].latitude, path[i + 1].longitude);

        float speed = std::min(30.0f, static_cast<float>(distance / 10.0));
        path[i].speed = std::max(5.0f, speed);
    }

    if (path.size() > 1) {
        path.back().bearing = path[path.size() - 2].bearing;
        path.back().speed = 0.0f;
    }
}
