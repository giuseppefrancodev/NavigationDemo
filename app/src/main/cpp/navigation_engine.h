#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "location_filter.h"
#include "route_matcher.h"
#include "road_graph.h"
#include "routing_engine.h"

class NavigationEngine {
public:
    NavigationEngine();
    ~NavigationEngine();
    
    // Core navigation methods
    RouteMatch updateLocation(double lat, double lon, float bearing, 
                             float speed, float accuracy);
    bool setDestination(double lat, double lon);
    std::vector<Route> getAlternativeRoutes() const;
    bool switchToRoute(const std::string& routeId);
    
private:
    // Core components
    std::unique_ptr<RouteMatcher> routeMatcher;
    std::unique_ptr<LocationFilter> locationFilter;
    std::unique_ptr<RoadGraph> roadGraph;
    std::unique_ptr<RoutingEngine> routingEngine;
    
    // State
    std::optional<Location> currentLocation;
    std::optional<Location> destinationLocation;
    std::vector<Route> alternativeRoutes;
    std::optional<Route> currentRoute;
};