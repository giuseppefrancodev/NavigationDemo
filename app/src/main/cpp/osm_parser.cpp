/*
 * File: osm_parser.cpp
 * Description: Implementation of the OSMParser class, responsible for parsing OSM (OpenStreetMap) data and building the road graph.
 * Author: Giuseppe Franco
 * Created: March 2025
 */

#include "osm_parser.h"
#include <android/log.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <random>
#include <pugixml.hpp>

#define LOG_TAG "OSMParser"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

extern void reportLoadingProgress(int progress);

OSMParser::OSMParser(RoadGraph* graph)
        : roadGraph(graph) {
    LOGI("OSMParser created");
}

bool OSMParser::parseOSMFile(const std::string& filePath) {
    LOGI("Parsing OSM file: %s using pugixml", filePath.c_str());

    std::ifstream file(filePath);
    if (!file.good()) {
        LOGE("OSM file not found: %s", filePath.c_str());
        return false;
    }

    int nodeCount = 0;
    int wayCount = 0;
    int roadCount = 0;

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(filePath.c_str());

    if (!result) {
        LOGE("Failed to parse OSM file: %s", result.description());
        return false;
    }

    LOGI("Processing nodes...");
    for (pugi::xml_node node : doc.child("osm").children("node")) {

        long long id = node.attribute("id").as_llong();
        double lat = node.attribute("lat").as_double();
        double lon = node.attribute("lon").as_double();

        Node* graphNode = roadGraph->addNode(std::to_string(id), lat, lon);
        osmNodeMap[id] = graphNode;

        nodeCount++;

        if (nodeCount % 10000 == 0) {
            LOGI("Processed %d nodes", nodeCount);
        }
    }

    LOGI("Processing ways...");
    for (pugi::xml_node way : doc.child("osm").children("way")) {

        long long id = way.attribute("id").as_llong();

        bool isRoad = false;
        std::unordered_map<std::string, std::string> tags;

        for (pugi::xml_node tag : way.children("tag")) {
            std::string key = tag.attribute("k").value();
            std::string value = tag.attribute("v").value();
            tags[key] = value;

            if (key == "highway") {
                isRoad = true;
            }
        }

        if (!isRoad) {
            continue;
        }

        std::vector<long long> nodeRefs;
        for (pugi::xml_node nd : way.children("nd")) {
            long long ref = nd.attribute("ref").as_llong();
            nodeRefs.push_back(ref);
        }

        processWay(id, nodeRefs, tags);

        roadCount++;
        wayCount++;

        if (wayCount % 1000 == 0) {
            LOGI("Processed %d ways (roads: %d)", wayCount, roadCount);
        }
    }

    LOGI("OSM parsing completed. Nodes: %d, Ways: %d, Roads: %d",
         nodeCount, wayCount, roadCount);

    return (nodeCount > 0 && roadCount > 0);
}

bool OSMParser::parseOSMPBF(const std::string& filePath) {
    LOGI("PBF parsing not implemented - using XML parser instead");

    return parseOSMFile(filePath);
}

RoadType OSMParser::getRoadTypeFromTags(
        const std::unordered_map<std::string, std::string>& tags) {

    auto it = tags.find("highway");
    if (it == tags.end()) {
        return RoadType::RESIDENTIAL;
    }

    const std::string& highway = it->second;

    if (highway == "motorway" || highway == "trunk" ||
        highway == "motorway_link" || highway == "trunk_link") {
        return RoadType::HIGHWAY;
    } else if (highway == "primary" || highway == "secondary" ||
               highway == "primary_link" || highway == "secondary_link") {
        return RoadType::PRIMARY;
    } else if (highway == "tertiary" || highway == "unclassified" ||
               highway == "tertiary_link") {
        return RoadType::SECONDARY;
    } else if (highway == "residential" || highway == "living_street") {
        return RoadType::RESIDENTIAL;
    } else if (highway == "service" || highway == "track") {
        return RoadType::SERVICE;
    }

    return RoadType::RESIDENTIAL;
}

double OSMParser::getSpeedLimitFromTags(
        const std::unordered_map<std::string, std::string>& tags,
        RoadType type) {

    auto it = tags.find("maxspeed");
    if (it != tags.end()) {
        try {
            return std::stod(it->second);
        } catch (...) {

        }
    }

    switch (type) {
        case RoadType::HIGHWAY:
            return 100.0;
        case RoadType::PRIMARY:
            return 70.0;
        case RoadType::SECONDARY:
            return 50.0;
        case RoadType::RESIDENTIAL:
            return 30.0;
        case RoadType::SERVICE:
            return 20.0;
    }

    return 30.0;
}

void OSMParser::processWay(
        long long wayId,
        const std::vector<long long>& nodeRefs,
        const std::unordered_map<std::string, std::string>& tags) {

    if (nodeRefs.size() < 2) {
        return;
    }

    auto highway = tags.find("highway");
    if (highway == tags.end()) {
        return;
    }

    const std::string& highwayType = highway->second;
    if (highwayType == "footway" || highwayType == "cycleway" ||
        highwayType == "path" || highwayType == "steps" ||
        highwayType == "pedestrian" || highwayType == "track" ||
        highwayType == "bus_guideway" || highwayType == "escape" ||
        highwayType == "raceway" || highwayType == "bridleway") {
        return;
    }

    auto access = tags.find("access");
    if (access != tags.end()) {
        const std::string& accessValue = access->second;
        if (accessValue == "private" || accessValue == "no") {
            return;
        }
    }

    RoadType roadType = getRoadTypeFromTags(tags);
    double speedLimit = getSpeedLimitFromTags(tags, roadType);

    std::string name = "Unnamed Road";
    auto nameTag = tags.find("name");
    if (nameTag != tags.end()) {
        name = nameTag->second;
    } else {

        auto refTag = tags.find("ref");
        if (refTag != tags.end()) {
            name = "Road " + refTag->second;
        }
    }

    bool isOneway = false;
    auto onewayTag = tags.find("oneway");
    if (onewayTag != tags.end()) {
        const std::string& onewayValue = onewayTag->second;
        isOneway = (onewayValue == "yes" || onewayValue == "true" || onewayValue == "1");
    }

    if (highwayType == "motorway" || highwayType == "motorway_link") {
        isOneway = true;
    }

    for (size_t i = 0; i < nodeRefs.size() - 1; i++) {
        long long fromId = nodeRefs[i];
        long long toId = nodeRefs[i + 1];

        if (osmNodeMap.find(fromId) == osmNodeMap.end() ||
            osmNodeMap.find(toId) == osmNodeMap.end()) {
            continue;
        }

        Node* fromNode = osmNodeMap[fromId];
        Node* toNode = osmNodeMap[toId];

        RoadSegment* segment = roadGraph->addSegment(fromNode, toNode, name, speedLimit, roadType);
        segment->isOneway = isOneway;

        if (!isOneway) {
            RoadSegment* reverseSegment = roadGraph->addSegment(toNode, fromNode, name, speedLimit, roadType);
            reverseSegment->isOneway = isOneway;
        }
    }
}
