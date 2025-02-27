#include "road_graph.h"
#include <android/log.h>
#include <cmath>

#define LOG_TAG "RoadGraph"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// Simple spatial index for demo purposes
class SpatialIndex {
public:
    void addSegment(RoadSegment* segment, double startLat, double startLon, double endLat, double endLon) {
        segments.push_back(segment);
    }
    
    std::vector<RoadSegment*> findNearby(double lat, double lon, double radius) {
        // For demo, we'll just return all segments
        return segments;
    }
    
private:
    std::vector<RoadSegment*> segments;
};

RoadGraph::RoadGraph() {
    LOGI("Creating RoadGraph");
    spatialIndex = std::make_unique<SpatialIndex>();
    createDemoNetwork();
}

RoadGraph::~RoadGraph() {
    LOGI("Destroying RoadGraph");
    // Memory is managed by unique_ptr
}

std::vector<RoadSegment*> RoadGraph::findNearbyRoads(const Location& loc, double radius) {
    return spatialIndex->findNearby(loc.latitude, loc.longitude, radius);
}

Node* RoadGraph::getNode(const std::string& id) {
    auto it = nodes.find(id);
    if (it != nodes.end()) {
        return it->second.get();
    }
    return nullptr;
}

void RoadGraph::createDemoNetwork() {
    // Create a simple grid of roads for demo purposes
    LOGI("Creating demo road network");
    
    // Create nodes in a grid pattern
    constexpr int gridSize = 10;
    constexpr double baseLatitude = 37.7749; // San Francisco
    constexpr double baseLongitude = -122.4194;
    constexpr double spacing = 0.001; // roughly 100 meters
    
    // Create grid nodes
    for (int i = 0; i < gridSize; i++) {
        for (int j = 0; j < gridSize; j++) {
            std::string id = "node_" + std::to_string(i) + "_" + std::to_string(j);
            double lat = baseLatitude + i * spacing;
            double lon = baseLongitude + j * spacing;
            addNode(id, lat, lon);
        }
    }
    
    // Connect nodes with road segments
    for (int i = 0; i < gridSize; i++) {
        for (int j = 0; j < gridSize; j++) {
            // Connect horizontally
            if (j < gridSize - 1) {
                Node* start = getNode("node_" + std::to_string(i) + "_" + std::to_string(j));
                Node* end = getNode("node_" + std::to_string(i) + "_" + std::to_string(j + 1));
                
                if (start && end) {
                    std::string name = "Street " + std::to_string(i);
                    RoadType type = (i % 3 == 0) ? RoadType::PRIMARY : RoadType::RESIDENTIAL;
                    double speedLimit = (i % 3 == 0) ? 50.0 : 30.0;
                    
                    addSegment(start, end, name, speedLimit, type);
                }
            }
            
            // Connect vertically
            if (i < gridSize - 1) {
                Node* start = getNode("node_" + std::to_string(i) + "_" + std::to_string(j));
                Node* end = getNode("node_" + std::to_string(i + 1) + "_" + std::to_string(j));
                
                if (start && end) {
                    std::string name = "Avenue " + std::to_string(j);
                    RoadType type = (j % 3 == 0) ? RoadType::PRIMARY : RoadType::RESIDENTIAL;
                    double speedLimit = (j % 3 == 0) ? 50.0 : 30.0;
                    
                    addSegment(start, end, name, speedLimit, type);
                }
            }
        }
    }
    
    LOGI("Created demo network with %zu nodes and %zu segments", nodes.size(), segments.size());
}

Node* RoadGraph::addNode(const std::string& id, double lat, double lon) {
    auto node = std::make_unique<Node>();
    node->id = id;
    node->latitude = lat;
    node->longitude = lon;
    
    Node* nodePtr = node.get();
    nodes[id] = std::move(node);
    return nodePtr;
}

RoadSegment* RoadGraph::addSegment(Node* start, Node* end, const std::string& name, 
                                 double speedLimit, RoadType type) {
    auto segment = std::make_unique<RoadSegment>();
    segment->start = start;
    segment->end = end;
    segment->name = name;
    segment->speedLimit = speedLimit;
    segment->type = type;
    segment->length = haversineDistance(
        start->latitude, start->longitude, 
        end->latitude, end->longitude
    );
    
    // Add to start node's outgoing segments
    start->segments.push_back(segment.get());
    
    // Add to spatial index
    spatialIndex->addSegment(
        segment.get(), 
        start->latitude, start->longitude,
        end->latitude, end->longitude
    );
    
    RoadSegment* segmentPtr = segment.get();
    segments.push_back(std::move(segment));
    return segmentPtr;
}

double RoadGraph::haversineDistance(double lat1, double lon1, double lat2, double lon2) {
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