/*
 * File: route_matcher.h
 * Description: Header file for the RouteMatcher class, defining route matching and navigation instruction generation.
 * Author: Giuseppe Franco
 * Created: March 2025
 */

#pragma once

#include <string>
#include <vector>
#include <optional>
#include "location_filter.h"
#include "road_graph.h"

struct RouteMatch {
    std::string streetName;
    std::string nextManeuver;
    int distanceToNext;
    std::string estimatedTimeOfArrival;
    double matchedLatitude;
    double matchedLongitude;
    float matchedBearing;
};

struct Route {
    std::string id;
    std::string name;
    std::vector<Location> points;
    int durationSeconds;
};

class RouteMatcher {
public:
    explicit RouteMatcher(RoadGraph* graph);

    RouteMatch match(const Location& loc);

    void setRoute(const Route& route);

private:
    RoadGraph* roadGraph;
    std::optional<Route> currentRoute;
    std::optional<Location> lastLocation;
    std::vector<double> cumulativeDistances;
    std::vector<RoadSegment*> routeSegments;

    int findClosestPointOnRoute(const Location& loc);
    double calculateMatchScore(const RoadSegment* segment, const Location& loc);
    Location projectOntoSegment(const Location& loc, const RoadSegment& segment);
    RouteMatch createRouteMatch(const Location& matched, const RoadSegment* segment, int closestPointIndex);
    int findNextManeuverPoint(int currentIndex);
    std::string determineNextManeuver(int currentIndex, int maneuverIndex);
    double calculateBearing(double lat1, double lon1, double lat2, double lon2);

    bool isSegmentOnRoute(RoadSegment* segment);
    void precalculateRouteSegments();
    double calculateSegmentToSegmentDistance(
            double lat1a, double lon1a, double lat1b, double lon1b,
            double lat2a, double lon2a, double lat2b, double lon2b);

    void validateRouteIntegrity();
};
