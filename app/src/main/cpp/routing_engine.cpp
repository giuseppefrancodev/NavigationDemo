#include "routing_engine.h"
#include <android/log.h>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <random>
#include <cmath>

#define LOG_TAG "RoutingEngine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

RoutingEngine::RoutingEngine(RoadGraph* graph)
        : roadGraph(graph) {
    LOGI("RoutingEngine created");
}

std::vector<Route> RoutingEngine::calculateRoutes(const Location& start, const Location& end) {
    LOGI("Calculating route from (%.6f, %.6f) to (%.6f, %.6f)",
         start.latitude, start.longitude, end.latitude, end.longitude);

    // 1) Find nearest nodes
    Node* startNode = findNearestNode(start);
    Node* endNode   = findNearestNode(end);

    if (!startNode || !endNode) {
        LOGE("Failed to find start or end node (null). No routes generated.");
        return {};
    }

    // 2) Find primary path using A*
    std::vector<Node*> primaryPath = findPath(startNode, endNode);
    if (primaryPath.empty()) {
        LOGE("Failed to find path via A*, returning empty routes.");
        return {};
    }

    // 3) Create main route
    Route primaryRoute = createRoute(primaryPath, generateRouteId());

    // 4) Generate alternatives for demonstration
    std::vector<Route> routes;
    routes.push_back(primaryRoute);

    auto altRoutes = generateAlternatives(primaryRoute);
    routes.insert(routes.end(), altRoutes.begin(), altRoutes.end());

    LOGI("Generated %zu routes", routes.size());
    return routes;
}

std::vector<Node*> RoutingEngine::findPath(Node* start, Node* end) {
    // If start==end, short-circuit
    if (start == end) {
        LOGI("A* findPath: start node == end node => single-node path");
        return {start};
    }

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

    openSet.push({ start, 0.0 });
    gScore[start] = 0.0;

    while (!openSet.empty()) {
        NodeData current = openSet.top();
        openSet.pop();

        if (current.node == end) {
            // Reconstruct path
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

        for (RoadSegment* segment : current.node->segments) {
            Node* neighbor = segment->end;
            if (closedSet.find(neighbor) != closedSet.end()) {
                continue; // Already processed
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

    // No path found
    return {};
}

Route RoutingEngine::createRoute(const std::vector<Node*>& path, const std::string& id) {
    Route route;
    route.id   = id;
    route.name = "Route to Destination"; // Example placeholder

    // Convert node path to route points
    for (Node* node : path) {
        Location point;
        point.latitude  = node->latitude;
        point.longitude = node->longitude;
        route.points.push_back(point);
    }

    // Calculate rough duration
    route.durationSeconds = calculateRouteDuration(route);
    return route;
}

double RoutingEngine::estimateHeuristic(const Node* current, const Node* goal) {
    // Direct distance (Haversine)
    return haversineDistance(
            current->latitude, current->longitude,
            goal->latitude,    goal->longitude
    );
}

std::string RoutingEngine::generateRouteId() {
    // Minimal pseudo-random hex string
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
    // Increase radius from 500 => 5000 for better chance to find roads
    const double searchRadiusMeters = 5000.0;

    // Log the location we're searching for
    LOGI("findNearestNode: location=(%.6f, %.6f). Searching up to %.1f meters.",
         location.latitude, location.longitude, searchRadiusMeters);

    std::vector<RoadSegment*> nearbyRoads = roadGraph->findNearbyRoads(location, searchRadiusMeters);

    LOGI("findNearestNode: found %zu roads within %.1f meters", nearbyRoads.size(), searchRadiusMeters);

    if (nearbyRoads.empty()) {
        LOGE("No roads found near (%.6f, %.6f)", location.latitude, location.longitude);
        return nullptr;
    }

    Node* nearest = nullptr;
    double minDistance = std::numeric_limits<double>::max();

    for (RoadSegment* segment : nearbyRoads) {
        double distStart = haversineDistance(location.latitude, location.longitude,
                                             segment->start->latitude, segment->start->longitude);
        LOGI("Candidate start node (%.6f, %.6f): dist=%.2f",
             segment->start->latitude, segment->start->longitude, distStart);

        if (distStart < minDistance) {
            minDistance = distStart;
            nearest     = segment->start;
        }

        double distEnd = haversineDistance(location.latitude, location.longitude,
                                           segment->end->latitude, segment->end->longitude);
        LOGI("Candidate end node   (%.6f, %.6f): dist=%.2f",
             segment->end->latitude, segment->end->longitude, distEnd);

        if (distEnd < minDistance) {
            minDistance = distEnd;
            nearest     = segment->end;
        }
    }

    if (nearest) {
        LOGI("findNearestNode => Nearest node is (%.6f, %.6f), dist=%.2f",
             nearest->latitude, nearest->longitude, minDistance);
    }
    return nearest;
}

std::vector<Route> RoutingEngine::generateAlternatives(const Route& primaryRoute) {
    // For demonstration, produce 1-2 alternative routes by slightly perturbing node positions
    std::vector<Route> alternatives;

    for (int i = 0; i < 2; i++) {
        Route alt = primaryRoute;
        alt.id    = generateRouteId();

        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(-0.0002, 0.0002); // ~20m offset

        for (auto & point : alt.points) {
            point.latitude  += dis(gen);
            point.longitude += dis(gen);
        }

        if (i == 0) {
            alt.durationSeconds = static_cast<int>(primaryRoute.durationSeconds * 0.9);
        } else {
            alt.durationSeconds = static_cast<int>(primaryRoute.durationSeconds * 1.15);
        }
        alternatives.push_back(alt);
    }
    return alternatives;
}

int RoutingEngine::calculateRouteDuration(const Route& route) {
    // Summation of distances between successive points
    double totalDistance = 0.0;
    for (size_t i = 1; i < route.points.size(); ++i) {
        const Location& p1 = route.points[i - 1];
        const Location& p2 = route.points[i];
        totalDistance += haversineDistance(p1.latitude, p1.longitude,
                                           p2.latitude, p2.longitude);
    }
    // e.g., average speed 35 km/h => ~9.72 m/s
    double avgSpeed = 9.72;
    int durationSec = static_cast<int>(totalDistance / avgSpeed);
    return durationSec;
}

double RoutingEngine::haversineDistance(double lat1, double lon1, double lat2, double lon2) {
    lat1 *= M_PI / 180.0;
    lon1 *= M_PI / 180.0;
    lat2 *= M_PI / 180.0;
    lon2 *= M_PI / 180.0;

    double dLat = lat2 - lat1;
    double dLon = lon2 - lon1;
    double a = std::sin(dLat / 2) * std::sin(dLat / 2)
               + std::cos(lat1) * std::cos(lat2)
                 * std::sin(dLon / 2) * std::sin(dLon / 2);
    double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
    double earthRadius = 6371000.0;
    return earthRadius * c;
}
