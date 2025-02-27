#pragma once

#include <chrono>
#include <optional>

// Location data structure
struct Location {
    double latitude;
    double longitude;
    float bearing;
    float speed;
    float accuracy;
};

class LocationFilter {
public:
    LocationFilter();

    // Process a raw location and return a filtered location
    Location process(const Location& raw);

private:
    bool initialized = false;

    // Simple state representation without Eigen
    double lat;         // Current latitude
    double lon;         // Current longitude
    double lat_vel;     // Latitude velocity
    double lon_vel;     // Longitude velocity

    // Covariance and noise values
    double positionVariance;
    double velocityVariance;
    double processNoisePosition;
    double processNoiseVelocity;
    double measurementNoise;

    // Last timestamp for velocity calculation
    long long lastTimestamp = 0;
};