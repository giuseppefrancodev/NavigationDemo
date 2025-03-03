/*
 * File: road_graph.cpp
 * Description: Implementation of the RoadGraph class, responsible for managing road network data and spatial indexing.
 * Author: Giuseppe Franco
 * Created: March 2025
 */

#include "road_graph.h"
#include "osm_parser.h"
#include <android/log.h>
#include <cmath>
#include <algorithm>
#include <queue>
#include <unordered_set>

#define LOG_TAG "RoadGraph"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

class SpatialIndex {
public:
    SpatialIndex(double cellSize = 0.001) : cellSize(cellSize) {
        LOGI("Created SpatialIndex with cell size %.5f degrees", cellSize);
    }

    void clear() {
        cells.clear();
        allSegments.clear();
        LOGI("SpatialIndex cleared");
    }

    void addSegment(RoadSegment* segment, double startLat, double startLon, double endLat, double endLon) {

        double minLat = std::min(startLat, endLat);
        double maxLat = std::max(startLat, endLat);
        double minLon = std::min(startLon, endLon);
        double maxLon = std::max(startLon, endLon);

        int minLatCell = static_cast<int>(minLat / cellSize);
        int maxLatCell = static_cast<int>(maxLat / cellSize);
        int minLonCell = static_cast<int>(minLon / cellSize);
        int maxLonCell = static_cast<int>(maxLon / cellSize);

        for (int latCell = minLatCell; latCell <= maxLatCell; latCell++) {
            for (int lonCell = minLonCell; lonCell <= maxLonCell; lonCell++) {
                std::string cellKey = std::to_string(latCell) + "," + std::to_string(lonCell);
                cells[cellKey].push_back(segment);
            }
        }

        allSegments.push_back(segment);
    }

    std::vector<RoadSegment*> findNearby(double lat, double lon, double radiusMeters) {

        int latCell = static_cast<int>(lat / cellSize);
        int lonCell = static_cast<int>(lon / cellSize);

        double degreesRadius = radiusMeters / 111000.0;
        int cellRadius = std::max(1, static_cast<int>(degreesRadius / cellSize) + 1);

        std::vector<RoadSegment*> result;
        std::unordered_set<RoadSegment*> segmentSet;

        for (int i = -cellRadius; i <= cellRadius; i++) {
            for (int j = -cellRadius; j <= cellRadius; j++) {
                std::string cellKey = std::to_string(latCell + i) + "," + std::to_string(lonCell + j);

                if (cells.find(cellKey) != cells.end()) {
                    for (RoadSegment* segment : cells[cellKey]) {
                        segmentSet.insert(segment);
                    }
                }
            }
        }

        if (segmentSet.empty() && radiusMeters > 1000.0) {
            LOGI("No segments found in spatial index cells, returning all segments (%zu)", allSegments.size());
            return allSegments;
        }

        result.insert(result.end(), segmentSet.begin(), segmentSet.end());

        LOGD("Found %zu nearby segments in spatial index", result.size());
        return result;
    }

private:
    double cellSize;
    std::unordered_map<std::string, std::vector<RoadSegment*>> cells;
    std::vector<RoadSegment*> allSegments;
};

RoadGraph::RoadGraph() {
    LOGI("Creating RoadGraph");
    spatialIndex = std::make_unique<SpatialIndex>(0.001);
    osmParser = std::make_unique<OSMParser>(this);
}

RoadGraph::~RoadGraph() {
    LOGI("Destroying RoadGraph");

}

void RoadGraph::clear() {
    LOGI("Clearing RoadGraph");
    nodes.clear();
    segments.clear();
    spatialIndex = std::make_unique<SpatialIndex>(0.001);
    nextSegmentId = 1;
}

std::vector<RoadSegment*> RoadGraph::findNearbyRoads(const Location& loc, double radius) {
    LOGD("Searching nearby roads at (%.6f, %.6f) within %.1f meters", loc.latitude, loc.longitude, radius);
    auto nearby = spatialIndex->findNearby(loc.latitude, loc.longitude, radius);
    LOGD("Found %zu nearby segments", nearby.size());
    return nearby;
}

Node* RoadGraph::getNode(const std::string& id) {
    auto it = nodes.find(id);
    if (it != nodes.end()) {
        return it->second.get();
    }
    return nullptr;
}

bool RoadGraph::loadOSMData(const std::string& filePath) {
    LOGI("Loading OSM data from file: %s", filePath.c_str());

    clear();

    std::string extension;
    size_t lastDot = filePath.find_last_of('.');
    if (lastDot != std::string::npos) {
        extension = filePath.substr(lastDot);
    }

    bool success = false;

    if (extension == ".pbf") {

        LOGI("Detected PBF format");
        success = osmParser->parseOSMPBF(filePath);
    } else {

        LOGI("Assuming XML format");
        success = osmParser->parseOSMFile(filePath);
    }

    if (!success) {
        LOGE("Failed to load OSM data");
        return false;
    }

    LOGI("Road graph contains %zu nodes and %zu segments",
         nodes.size(), segments.size());

    return true;
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
    segment->id = nextSegmentId++;

    start->segments.push_back(segment.get());

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

    lat1 *= M_PI / 180.0;
    lon1 *= M_PI / 180.0;
    lat2 *= M_PI / 180.0;
    lon2 *= M_PI / 180.0;

    double dLat = lat2 - lat1;
    double dLon = lon2 - lon1;
    double a = std::sin(dLat / 2) * std::sin(dLat / 2) +
               std::cos(lat1) * std::cos(lat2) *
               std::sin(dLon / 2) * std::sin(dLon / 2);
    double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));

    double earthRadius = 6371000.0;
    return earthRadius * c;
}
