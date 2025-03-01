#include "location_filter.h"
#include <android/log.h>
#include <cmath>

#define LOG_TAG "LocationFilter"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

// Constants for filter tuning
constexpr double INITIAL_POSITION_VARIANCE = 10.0;
constexpr double INITIAL_VELOCITY_VARIANCE = 5.0;
constexpr double PROCESS_NOISE_POSITION    = 0.01; // Position process noise
constexpr double PROCESS_NOISE_VELOCITY    = 0.1;  // Velocity process noise
constexpr double BASE_MEASUREMENT_NOISE    = 5.0;  // Base measurement noise

LocationFilter::LocationFilter() {
    LOGI("LocationFilter created");

    // Initialize state
    lat     = 0.0;
    lon     = 0.0;
    lat_vel = 0.0;
    lon_vel = 0.0;

    // Initialize variances and noise values
    positionVariance    = INITIAL_POSITION_VARIANCE;
    velocityVariance    = INITIAL_VELOCITY_VARIANCE;
    processNoisePosition= PROCESS_NOISE_POSITION;
    processNoiseVelocity= PROCESS_NOISE_VELOCITY;
    measurementNoise    = BASE_MEASUREMENT_NOISE;

    lastTimestamp       = 0;
}

Location LocationFilter::process(const Location& raw) {
    auto now   = std::chrono::system_clock::now();
    auto nowMs = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    long long currentTimestamp = nowMs.time_since_epoch().count();

    if (!initialized) {
        // First location, just use as-is
        lat  = raw.latitude;
        lon  = raw.longitude;
        lat_vel = 0.0;
        lon_vel = 0.0;
        initialized = true;
        lastTimestamp = currentTimestamp;

        LOGI("Filter initialized with location: %.6f, %.6f", lat, lon);
        return raw;
    }

    // Calculate time delta
    double dt = (currentTimestamp - lastTimestamp) / 1000.0; // seconds
    if (dt <= 0 || dt > 10.0) {
        LOGI("Invalid time delta: %.3f seconds, resetting to 0.1", dt);
        dt = 0.1;
    }
    lastTimestamp = currentTimestamp;

    // Adaptive measurement noise based on accuracy
    double adaptedNoise = BASE_MEASUREMENT_NOISE;
    if (raw.accuracy > 0) {
        adaptedNoise = BASE_MEASUREMENT_NOISE * (raw.accuracy / 10.0);
    }

    // Predict step (simplified Kalman filter)
    double predictedLat = lat + lat_vel * dt;
    double predictedLon = lon + lon_vel * dt;

    double predictedPosVar = positionVariance + processNoisePosition + velocityVariance * dt * dt;
    double predictedVelVar = velocityVariance + processNoiseVelocity;

    // Update step (simplified Kalman filter)
    double kLat = predictedPosVar / (predictedPosVar + adaptedNoise);
    double kLon = predictedPosVar / (predictedPosVar + adaptedNoise);

    // Clamp Kalman gains
    kLat = std::max(0.1, std::min(kLat, 0.9));
    kLon = std::max(0.1, std::min(kLon, 0.9));

    // Update state with measurement
    lat = predictedLat + kLat * (raw.latitude - predictedLat);
    lon = predictedLon + kLon * (raw.longitude - predictedLon);

    // Velocity update
    double new_lat_vel = (raw.latitude - predictedLat) / dt;
    double new_lon_vel = (raw.longitude - predictedLon) / dt;

    // Detect extreme velocity changes
    const double MAX_VELOCITY_CHANGE = 10.0;
    if (std::abs(new_lat_vel - lat_vel) > MAX_VELOCITY_CHANGE) {
        new_lat_vel = lat_vel + std::copysign(MAX_VELOCITY_CHANGE, new_lat_vel - lat_vel);
    }
    if (std::abs(new_lon_vel - lon_vel) > MAX_VELOCITY_CHANGE) {
        new_lon_vel = lon_vel + std::copysign(MAX_VELOCITY_CHANGE, new_lon_vel - lon_vel);
    }

    // Smooth velocity
    lat_vel = lat_vel * 0.7 + new_lat_vel * 0.3;
    lon_vel = lon_vel * 0.7 + new_lon_vel * 0.3;

    // Update variances
    positionVariance = (1 - kLat) * predictedPosVar;
    velocityVariance = (1 - kLat) * predictedVelVar;

    // Calculate bearing & speed
    float calculatedBearing = raw.bearing;
    float calculatedSpeed   = raw.speed;

    double velocityMagnitude = std::sqrt(lat_vel * lat_vel + lon_vel * lon_vel);
    if (velocityMagnitude > 0.00001) {
        calculatedBearing = static_cast<float>(std::atan2(lon_vel, lat_vel) * 180.0 / M_PI);
        if (calculatedBearing < 0) {
            calculatedBearing += 360.0f;
        }
        // Approx: 1 deg of lat ~111km, so velocityMagnitude * 111000
        calculatedSpeed = static_cast<float>(velocityMagnitude * 111000);
    }

    // Return filtered location
    Location filtered;
    filtered.latitude  = lat;
    filtered.longitude = lon;
    // If raw bearing/speed is NaN, use computed; otherwise keep raw
    filtered.bearing   = std::isnan(raw.bearing) ? calculatedBearing : raw.bearing;
    filtered.speed     = std::isnan(raw.speed)   ? calculatedSpeed   : raw.speed;
    filtered.accuracy  = raw.accuracy * 0.8f;

    LOGD("Filtered location: %.6f, %.6f (from %.6f, %.6f), bearing: %.1f, speed: %.1f",
         filtered.latitude, filtered.longitude, raw.latitude, raw.longitude,
         filtered.bearing, filtered.speed);

    return filtered;
}
