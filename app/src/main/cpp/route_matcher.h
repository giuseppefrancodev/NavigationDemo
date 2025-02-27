#pragma once

#include <string>
#include <vector>
#include <optional>
#include "location_filter.h"
#include "road_graph.h"

// Match result data
struct RouteMatch {
    std::string streetName;
    std::string nextManeuver;
    int distanceToNext;
    std::string estimatedTimeOfArrival;
    double matchedLatitude;
    double matchedLongitude;
    float matchedBearing;
};

// Route data structure
struct Route {
    std::string id;
    std::string name;
    std::vector<Location> points;
    int durationSeconds;
};

class RouteMatcher {
public:
    explicit RouteMatcher(RoadGraph* graph);
    
    // Match a location to the current route
    RouteMatch match(const Location& loc);
    
    // Set the active route for matching
    void setRoute(const Route& route);
    
    // Get current location (last matched)
    Location getCurrentLocation() const;
    
private:
    RoadGraph* roadGraph;
    std::optional<Route> currentRoute;
    std::optional<Location> lastLocation;
    
    // Helper methods
    double calculateMatchScore(const RoadSegment* segment, const Location& loc);
    Location projectOntoSegment(const Location& loc, const RoadSegment& segment);
    RouteMatch createRouteMatch(const Location& matched, const RoadSegment* segment);
};