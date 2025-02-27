#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include "location_filter.h"

// Forward declarations
class SpatialIndex;

// Road type enum
enum class RoadType {
    HIGHWAY,
    PRIMARY,
    SECONDARY,
    RESIDENTIAL,
    SERVICE
};

// Road graph structures
struct Node;

struct RoadSegment {
    Node* start;
    Node* end;
    std::string name;
    double speedLimit;
    RoadType type;
    double length;  // meters
};

struct Node {
    std::string id;
    double latitude;
    double longitude;
    std::vector<RoadSegment*> segments;
};

class RoadGraph {
public:
    RoadGraph();
    ~RoadGraph();
    
    // Find nearby road segments within radius
    std::vector<RoadSegment*> findNearbyRoads(const Location& loc, double radius);
    
    // Get node by ID
    Node* getNode(const std::string& id);
    
    // Create a demo road network for testing
    void createDemoNetwork();
    
private:
    std::unordered_map<std::string, std::unique_ptr<Node>> nodes;
    std::vector<std::unique_ptr<RoadSegment>> segments;
    std::unique_ptr<SpatialIndex> spatialIndex;
    
    // Helper to add a node
    Node* addNode(const std::string& id, double lat, double lon);
    
    // Helper to add a road segment
    RoadSegment* addSegment(Node* start, Node* end, const std::string& name, 
                          double speedLimit, RoadType type);
                          
    // Calculate distance between two points
    static double haversineDistance(double lat1, double lon1, double lat2, double lon2);
};