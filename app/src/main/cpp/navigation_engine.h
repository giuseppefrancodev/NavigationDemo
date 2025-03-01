#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "location_filter.h"
#include "route_matcher.h"
#include "road_graph.h"
#include "routing_engine.h"

/**
 * Represents a high-level navigation engine that combines:
 *  - Filtering of raw location data (LocationFilter)
 *  - Route calculation (RoutingEngine)
 *  - Route matching to track progress (RouteMatcher)
 *  - Basic utilities (e.g., Haversine distance)
 */
class NavigationEngine {
public:
    NavigationEngine();
    ~NavigationEngine();

    // Periodic update with current device location
    RouteMatch updateLocation(double lat, double lon, float bearing,
                              float speed, float accuracy);

    // Set a new route destination
    bool setDestination(double lat, double lon);

    // Retrieve current alternative routes
    std::vector<Route> getAlternativeRoutes() const;

    // Switch active route by routeId
    bool switchToRoute(const std::string& routeId);

    // Simple path generator (not road-aware)
    std::vector<Location> getDetailedPath(
            double startLat, double startLon,
            double endLat, double endLon,
            int maxSegments);

private:
    // Core components
    std::unique_ptr<RouteMatcher>   routeMatcher;
    std::unique_ptr<LocationFilter> locationFilter;
    std::unique_ptr<RoadGraph>      roadGraph;
    std::unique_ptr<RoutingEngine>  routingEngine;

    // State
    std::optional<Location>         currentLocation;
    std::optional<Location>         destinationLocation;
    std::vector<Route>              alternativeRoutes;
    std::optional<Route>            currentRoute;

    // Internal helpers
    void   calculateBearingAndSpeed(std::vector<Location>& path);
    double haversineDistance(double lat1, double lon1, double lat2, double lon2);
};
