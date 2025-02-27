#include "routing_engine.h"
#include <android/log.h>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <random>
#include <cmath>

#define LOG_TAG "RoutingEngine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

RoutingEngine::RoutingEngine(RoadGraph* graph) 
    : roadGraph(graph) {
    LOGI("RoutingEngine created");
}

std::vector<Route> RoutingEngine::calculateRoutes(const Location& start, const Location& end) {
    LOGI("Calculating route from (%.6f, %.6f) to (%.6f, %.6f)",
         start.latitude, start.longitude, end.latitude, end.longitude);
    
    // Find nearest nodes to start and end
    Node* startNode = findNearestNode(start);
    Node* endNode = findNearestNode(end);
    
    if (!startNode || !endNode) {
        LOGE("Failed to find start or end node");
        return {};
    }
    
    // Find primary route using A*
    std::vector<Node*> primaryPath = findPath(startNode, endNode);
    
    if (primaryPath.empty()) {
        LOGE("Failed to find path");
        return {};
    }
    
    // Create primary route
    Route primaryRoute = createRoute(primaryPath, generateRouteId());
    
    // Generate alternative routes
    std::vector<Route> routes = {primaryRoute};
    
    auto alternatives = generateAlternatives(primaryRoute);
    routes.insert(routes.end(), alternatives.begin(), alternatives.end());
    
    LOGI("Generated %zu routes", routes.size());
    return routes;
}

std::vector<Node*> RoutingEngine::findPath(Node* start, Node* end) {
    // A* algorithm
    struct NodeData {
        Node* node;
        double fScore; // g + h
        
        bool operator>(const NodeData& other) const {
            return fScore > other.fScore;
        }
    };
    
    std::priority_queue<NodeData, std::vector<NodeData>, std::greater<NodeData>> openSet;
    std::unordered_set<Node*> closedSet;
    std::unordered_map<Node*, Node*> cameFrom;
    std::unordered_map<Node*, double> gScore;
    
    // Initialize
    openSet.push({start, 0});
    gScore[start] = 0;
    
    while (!openSet.empty()) {
        NodeData current = openSet.top();
        openSet.pop();
        
        if (current.node == end) {
            // Found the path, reconstruct it
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
            continue; // Already processed
        }
        
        closedSet.insert(current.node);
        
        // Process neighbors
        for (RoadSegment* segment : current.node->segments) {
            Node* neighbor = segment->end;
            
            if (closedSet.find(neighbor) != closedSet.end()) {
                continue; // Already processed
            }
            
            // Cost to neighbor
            double tentativeG = gScore[current.node] + segment->length;
            
            if (gScore.find(neighbor) == gScore.end() || tentativeG < gScore[neighbor]) {
                // This is a better path
                cameFrom[neighbor] = current.node;
                gScore[neighbor] = tentativeG;
                double heuristic = estimateHeuristic(neighbor, end);
                openSet.push({neighbor, tentativeG + heuristic});
            }
        }
    }
    
    // No path found
    return {};
}

Route RoutingEngine::createRoute(const std::vector<Node*>& path, const std::string& id) {
    Route route;
    route.id = id;
    route.name = "Route to Destination"; // Placeholder
    
    // Convert path to route points
    for (Node* node : path) {
        Location point;
        point.latitude = node->latitude;
        point.longitude = node->longitude;
        route.points.push_back(point);
    }
    
    // Calculate duration
    route.durationSeconds = calculateRouteDuration(route);
    
    return route;
}

double RoutingEngine::estimateHeuristic(const Node* current, const Node* goal) {
    // Simple heuristic: direct distance (as the crow flies)
    double lat1 = current->latitude;
    double lon1 = current->longitude;
    double lat2 = goal->latitude;
    double lon2 = goal->longitude;
    
    // Convert to radians
    lat1 *= M_PI / 180.0;
    lon1 *= M_PI / 180.0;
    lat2 *= M_PI / 180.0;
    lon2 *= M_PI / 180.0;
    
    // Haversine formula
    double dLat = lat2 - lat1;
    double dLon = lon2 - lon1;
    double a = std::sin(dLat / 2) * std::sin(dLat / 2) +
               std::cos(lat1) * std::cos(lat2) *
               std::sin(dLon / 2) * std::sin(dLon / 2);
    double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
    
    // Earth radius in meters
    double earthRadius = 6371000.0;
    return earthRadius * c;
}

std::string RoutingEngine::generateRouteId() {
    // Simple UUID-like generator
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

Node* RoutingEngine::findNearestNode(const Location& location) {
    // Find nearest node in the graph
    std::vector<RoadSegment*> nearbyRoads = roadGraph->findNearbyRoads(location, 500.0);
    
    if (nearbyRoads.empty()) {
        return nullptr;
    }
    
    Node* nearest = nullptr;
    double minDistance = std::numeric_limits<double>::max();
    
    for (RoadSegment* segment : nearbyRoads) {
        // Check start node
        double distToStart = haversineDistance(
            location.latitude, location.longitude,
            segment->start->latitude, segment->start->longitude
        );
        
        if (distToStart < minDistance) {
            minDistance = distToStart;
            nearest = segment->start;
        }
        
        // Check end node
        double distToEnd = haversineDistance(
            location.latitude, location.longitude,
            segment->end->latitude, segment->end->longitude
        );
        
        if (distToEnd < minDistance) {
            minDistance = distToEnd;
            nearest = segment->end;
        }
    }
    
    return nearest;
}

std::vector<Route> RoutingEngine::generateAlternatives(const Route& primaryRoute) {
    // Generate 1-2 alternative routes for demo purposes
    std::vector<Route> alternatives;
    
    // For demo, just create slight variations of the primary route
    for (int i = 0; i < 2; ++i) {
        Route alt = primaryRoute;
        alt.id = generateRouteId();
        
        // Perturb the points slightly
        for (auto& point : alt.points) {
            // Add small random offsets
            static std::random_device rd;
            static std::mt19937 gen(rd());
            std::uniform_real_distribution<> dis(-0.0002, 0.0002); // About 20m
            
            point.latitude += dis(gen);
            point.longitude += dis(gen);
        }
        
        // Adjust duration
        if (i == 0) {
            // Faster route
            alt.durationSeconds = static_cast<int>(primaryRoute.durationSeconds * 0.9);
        } else {
            // Slower route
            alt.durationSeconds = static_cast<int>(primaryRoute.durationSeconds * 1.15);
        }
        
        alternatives.push_back(alt);
    }
    
    return alternatives;
}

int RoutingEngine::calculateRouteDuration(const Route& route) {
    // Calculate route duration based on distance and average speed
    double totalDistance = 0.0;
    
    for (size_t i = 1; i < route.points.size(); ++i) {
        const Location& prev = route.points[i - 1];
        const Location& curr = route.points[i];
        
        totalDistance += haversineDistance(
            prev.latitude, prev.longitude,
            curr.latitude, curr.longitude
        );
    }
    
    // Assume average speed of 35 km/h (9.72 m/s)
    double avgSpeed = 9.72;
    
    // Duration in seconds
    return static_cast<int>(totalDistance / avgSpeed);
}

double RoutingEngine::haversineDistance(double lat1, double lon1, double lat2, double lon2) {
    // Convert to radians
    lat1 *= M_PI / 180.0;
    lon1 *= M_PI / 180.0;
    lat2 *= M_PI / 180.0;
    lon2 *= M_PI / 180.0;
    
    // Haversine formula
    double dLat = lat2 - lat1;
    double dLon = lon2 - lon1;
    double a = std::sin(dLat / 2) * std::sin(dLat / 2) +
               std::cos(lat1) * std::cos(lat2) *
               std::sin(dLon / 2) * std::sin(dLon / 2);
    double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
    
    // Earth radius in meters
    double earthRadius = 6371000.0;
    return earthRadius * c;
}