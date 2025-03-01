#pragma once

#include <chrono>
#include <optional>

// Location data structure
struct Location {
    double latitude   = 0.0;
    double longitude  = 0.0;
    float bearing     = 0.0f;
    float speed       = 0.0f;
    float accuracy    = 0.0f;

    // Added constructor for convenience
    Location() = default;

    Location(double lat, double lon, float b, float s, float a = 0.0f)
            : latitude(lat), longitude(lon), bearing(b), speed(s), accuracy(a) {}
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
