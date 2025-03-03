/*
 * File: routing_engine.h
 * Description: Header file for the RoutingEngine class, defining the structure for route calculation and pathfinding.
 * Author: Giuseppe Franco
 * Created: March 2025
 */

#pragma once

#include <memory>
#include <vector>
#include <string>
#include <functional>
#include "road_graph.h"
#include "route_matcher.h"

class RoutingEngine {
public:
    explicit RoutingEngine(RoadGraph* graph);

    std::vector<Route> calculateRoutes(const Location& start, const Location& end);

private:
    RoadGraph* roadGraph;

    struct NodeData {
        Node* node;
        double fScore;
        bool operator>(const NodeData& other) const {
            return fScore > other.fScore;
        }
    };

    void addIntermediatePoints(std::vector<Location>& points,
                               const Location& start,
                               const Location& end,
                               int count);

    std::vector<Location> douglasPeucker(const std::vector<Location>& points, double epsilon);

    double perpendicularDistance(const Location& point, const Location& lineStart, const Location& lineEnd);

    void calculateBearingAndSpeed(std::vector<Location>& path);

    std::vector<Node*> findPath(Node* start, Node* end);

    Route createDetailedRoute(const std::vector<Node*>& path, const std::string& id,
                              const Location& start, const Location& end);

    Route createDirectRoute(const Location& start, const Location& end);

    double estimateHeuristic(const Node* current, const Node* goal);

    std::string generateRouteId();

    Node* findNearestNode(const Location& location, double searchRadius = 5000.0);

    std::vector<Route> generateAlternatives(const Route& primaryRoute,
                                            const Location& start,
                                            const Location& end);

    Route generateFastRoute(Node* start, Node* end,
                            const Location& startLoc,
                            const Location& endLoc);

    Route generateNoHighwaysRoute(Node* start, Node* end,
                                  const Location& startLoc,
                                  const Location& endLoc);

    std::vector<Node*> findPathWithCostFunction(
            Node* start,
            Node* end,
            std::function<double(RoadSegment*)> costFunction);

    bool isRouteDifferentEnough(const Route& route1, const Route& route2);

    Location getRoutePointAtFraction(const Route& route, double fraction);

    int calculateRouteDuration(const Route& route);
    int calculateCustomDuration(const Route& route, double speedFactor);

    Location projectLocationOntoSegment(const Location& loc, RoadSegment* segment);

    RoadSegment* findConnectingSegment(Node* from, Node* to);

    void smoothRoutePath(Route& route);

    double calculateBearing(double lat1, double lon1, double lat2, double lon2);
};
