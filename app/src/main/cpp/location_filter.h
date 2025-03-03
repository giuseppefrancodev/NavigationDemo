/*
 * File: location_filter.h
 * Description: Header file for the LocationFilter class, defining the structure for filtering and smoothing location data.
 * Author: Giuseppe Franco
 * Created: March 2025
 */

#pragma once

#include <chrono>
#include <optional>

struct Location {
    double latitude   = 0.0;
    double longitude  = 0.0;
    float bearing     = 0.0f;
    float speed       = 0.0f;
    float accuracy    = 0.0f;

    Location() = default;

    Location(double lat, double lon, float b, float s, float a = 0.0f)
            : latitude(lat), longitude(lon), bearing(b), speed(s), accuracy(a) {}
};

class LocationFilter {
public:
    LocationFilter();

    Location process(const Location& raw);

private:
    bool initialized = false;

    double lat;
    double lon;
    double lat_vel;
    double lon_vel;

    double positionVariance;
    double velocityVariance;
    double processNoisePosition;
    double processNoiseVelocity;

    long long lastTimestamp = 0;
};
