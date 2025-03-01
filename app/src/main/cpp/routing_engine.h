#pragma once

#include <memory>
#include <vector>
#include <string>
#include "road_graph.h"
#include "route_matcher.h"

class RoutingEngine {
public:
    explicit RoutingEngine(RoadGraph* graph);

    // Calculate routes between two points
    std::vector<Route> calculateRoutes(const Location& start, const Location& end);

private:
    RoadGraph* roadGraph;

    // A* routing implementation
    std::vector<Node*> findPath(Node* start, Node* end);

    // Convert path of nodes to a Route
    Route createRoute(const std::vector<Node*>& path, const std::string& id);

    // Heuristic function for A*
    double estimateHeuristic(const Node* current, const Node* goal);

    // Generate unique route ID
    std::string generateRouteId();

    // Helper method to find nearest node
    Node* findNearestNode(const Location& location);

    // Generate alternative routes
    std::vector<Route> generateAlternatives(const Route& primaryRoute);

    // Calculate route duration
    int calculateRouteDuration(const Route& route);

    // Helper function for distance calculation
    double haversineDistance(double lat1, double lon1, double lat2, double lon2);
};
