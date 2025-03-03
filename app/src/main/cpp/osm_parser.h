/*
 * File: osm_parser.h
 * Description: Header file for the OSMParser class, defining the structure for parsing OSM data and building the road network.
 * Author: Giuseppe Franco
 * Created: March 2025
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "road_graph.h"

class OSMParser {
public:
    OSMParser(RoadGraph* graph);

    bool parseOSMFile(const std::string& filePath);

    bool parseOSMPBF(const std::string& filePath);

private:
    RoadGraph* roadGraph;

    std::unordered_map<long long, Node*> osmNodeMap;

    void processWay(
            long long wayId,
            const std::vector<long long>& nodeRefs,
            const std::unordered_map<std::string, std::string>& tags
    );

    RoadType getRoadTypeFromTags(
            const std::unordered_map<std::string, std::string>& tags
    );

    double getSpeedLimitFromTags(
            const std::unordered_map<std::string, std::string>& tags,
            RoadType type
    );
};
