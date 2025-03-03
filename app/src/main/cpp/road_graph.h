/*
 * File: road_graph.h
 * Description: Header file for the RoadGraph class, defining road network structures and spatial indexing.
 * Author: Giuseppe Franco
 * Created: March 2025
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include "location_filter.h"

class SpatialIndex;
class OSMParser;

enum class RoadType {
    HIGHWAY,
    PRIMARY,
    SECONDARY,
    RESIDENTIAL,
    SERVICE
};

struct Node;

struct RoadSegment {
    Node* start;
    Node* end;
    std::string name;
    double speedLimit;
    RoadType type;
    double length;
    int id;

    bool isOneway = false;
    float priority = 1.0f;
    std::vector<std::pair<RoadSegment*, double>> turnCosts;
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

    std::vector<RoadSegment*> findNearbyRoads(const Location& loc, double radius);

    Node* getNode(const std::string& id);

    bool loadOSMData(const std::string& filePath);

    size_t getNodesCount() const { return nodes.size(); }
    size_t getSegmentsCount() const { return segments.size(); }

    Node* addNode(const std::string& id, double lat, double lon);

    RoadSegment* addSegment(Node* start, Node* end, const std::string& name,
                            double speedLimit, RoadType type);

    static double haversineDistance(double lat1, double lon1, double lat2, double lon2);

    void clear();

private:
    std::unordered_map<std::string, std::unique_ptr<Node>> nodes;
    std::vector<std::unique_ptr<RoadSegment>> segments;
    std::unique_ptr<SpatialIndex> spatialIndex;
    std::unique_ptr<OSMParser> osmParser;

    int nextSegmentId = 1;
};
