/*
 * File: navigation_engine.h
 * Description: Header file for the NavigationEngine class, defining the core navigation logic and route management.
 * Author: Giuseppe Franco
 * Created: March 2025
 */

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <android/asset_manager.h>
#include "location_filter.h"
#include "route_matcher.h"
#include "road_graph.h"
#include "routing_engine.h"

class NavigationEngine {
public:
    NavigationEngine();
    ~NavigationEngine();

    RouteMatch updateLocation(double lat, double lon, float bearing,
                              float speed, float accuracy);

    bool setDestination(double lat, double lon);

    std::vector<Route> getAlternativeRoutes() const;

    bool switchToRoute(const std::string& routeId);

    std::vector<Location> getDetailedPath(
            double startLat, double startLon,
            double endLat, double endLon,
            int maxSegments);

    bool loadOSMFromAssets(AAssetManager* assetManager, const std::string& fileName);

private:

    std::unique_ptr<RouteMatcher>   routeMatcher;
    std::unique_ptr<LocationFilter> locationFilter;
    std::unique_ptr<RoadGraph>      roadGraph;
    std::unique_ptr<RoutingEngine>  routingEngine;

    std::optional<Location>         currentLocation;
    std::optional<Location>         destinationLocation;
    std::vector<Route>              alternativeRoutes;
    std::optional<Route>            currentRoute;

    void   calculateBearingAndSpeed(std::vector<Location>& path);
    static double haversineDistance(double lat1, double lon1, double lat2, double lon2);
};
