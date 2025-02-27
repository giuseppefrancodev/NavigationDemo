#include "location_filter.h"
#include <android/log.h>
#include <cmath>

#define LOG_TAG "LocationFilter"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

// Constants for filter tuning - extracted for easier adjustment
constexpr double INITIAL_POSITION_VARIANCE = 10.0;
constexpr double INITIAL_VELOCITY_VARIANCE = 5.0;
constexpr double PROCESS_NOISE_POSITION = 0.01;  // Position process noise
constexpr double PROCESS_NOISE_VELOCITY = 0.1;   // Velocity process noise
constexpr double BASE_MEASUREMENT_NOISE = 5.0;   // Base measurement noise

LocationFilter::LocationFilter() {
    LOGI("LocationFilter created");

    // Initialize state
    lat = 0.0;
    lon = 0.0;
    lat_vel = 0.0;
    lon_vel = 0.0;

    // Initialize variances and noise values
    positionVariance = INITIAL_POSITION_VARIANCE;
    velocityVariance = INITIAL_VELOCITY_VARIANCE;
    processNoisePosition = PROCESS_NOISE_POSITION;
    processNoiseVelocity = PROCESS_NOISE_VELOCITY;
    measurementNoise = BASE_MEASUREMENT_NOISE;

    lastTimestamp = 0;
}

Location LocationFilter::process(const Location& raw) {
    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    auto nowMs = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    long long currentTimestamp = nowMs.time_since_epoch().count();

    if (!initialized) {
        // First location, just use as-is
        lat = raw.latitude;
        lon = raw.longitude;
        lat_vel = 0.0;
        lon_vel = 0.0;
        initialized = true;
        lastTimestamp = currentTimestamp;

        LOGI("Filter initialized with location: %.6f, %.6f", lat, lon);
        return raw;
    }

    // Calculate time delta
    double dt = (currentTimestamp - lastTimestamp) / 1000.0; // seconds

    // Handle potential clock issues or very large time gaps
    if (dt <= 0 || dt > 10.0) {
        LOGI("Invalid time delta: %.3f seconds, resetting to 0.1", dt);
        dt = 0.1;
    }

    // Update last timestamp
    lastTimestamp = currentTimestamp;

    // Adaptive measurement noise based on accuracy
    // Higher accuracy (lower value) = lower noise value = more weight to measurement
    double adaptedNoise = BASE_MEASUREMENT_NOISE;
    if (raw.accuracy > 0) {
        adaptedNoise = BASE_MEASUREMENT_NOISE * (raw.accuracy / 10.0);
    }

    // Predict step (simplified Kalman filter)
    // Predict new state based on velocity
    double predictedLat = lat + lat_vel * dt;
    double predictedLon = lon + lon_vel * dt;

    // Update variances for prediction
    double predictedPosVar = positionVariance + processNoisePosition + velocityVariance * dt * dt;
    double predictedVelVar = velocityVariance + processNoiseVelocity;

    // Update step (simplified Kalman filter)
    // Calculate Kalman gains
    double kLat = predictedPosVar / (predictedPosVar + adaptedNoise);
    double kLon = predictedPosVar / (predictedPosVar + adaptedNoise);

    // Clamp Kalman gain to reasonable values
    kLat = std::max(0.1, std::min(kLat, 0.9));
    kLon = std::max(0.1, std::min(kLon, 0.9));

    // Update state with measurement
    lat = predictedLat + kLat * (raw.latitude - predictedLat);
    lon = predictedLon + kLon * (raw.longitude - predictedLon);

    // Calculate new velocities with some smoothing
    double new_lat_vel = (raw.latitude - predictedLat) / dt;
    double new_lon_vel = (raw.longitude - predictedLon) / dt;

    // Extreme velocity change detection (for outlier rejection)
    const double MAX_VELOCITY_CHANGE = 10.0; // m/s²
    if (std::abs(new_lat_vel - lat_vel) > MAX_VELOCITY_CHANGE ||
        std::abs(new_lon_vel - lon_vel) > MAX_VELOCITY_CHANGE) {
        LOGI("Detected possible GPS jump, limiting velocity change");
        new_lat_vel = lat_vel + std::copysign(MAX_VELOCITY_CHANGE, new_lat_vel - lat_vel);
        new_lon_vel = lon_vel + std::copysign(MAX_VELOCITY_CHANGE, new_lon_vel - lon_vel);
    }

    // Update velocities with smoothing
    lat_vel = lat_vel * 0.7 + new_lat_vel * 0.3;
    lon_vel = lon_vel * 0.7 + new_lon_vel * 0.3;

    // Update variance estimates
    positionVariance = (1 - kLat) * predictedPosVar;
    velocityVariance = (1 - kLat) * predictedVelVar;

    // Calculate bearing from velocity components if speed is sufficient
    float calculatedBearing = raw.bearing;
    float calculatedSpeed = raw.speed;

    // Only calculate bearing from velocity if we're moving fast enough
    double velocityMagnitude = std::sqrt(lat_vel * lat_vel + lon_vel * lon_vel);
    if (velocityMagnitude > 0.00001) { // Approx > 1 m/s
        calculatedBearing = static_cast<float>(std::atan2(lon_vel, lat_vel) * 180.0 / M_PI);
        if (calculatedBearing < 0) {
            calculatedBearing += 360.0f;
        }

        // Estimate speed from velocity (rough approximation)
        // 0.00001 in lat/lon is roughly 1.1m at equator
        calculatedSpeed = static_cast<float>(velocityMagnitude * 111000);
    }

    // Return filtered location
    Location filtered;
    filtered.latitude = lat;
    filtered.longitude = lon;
    filtered.bearing = std::isnan(raw.bearing) ? calculatedBearing : raw.bearing;
    filtered.speed = std::isnan(raw.speed) ? calculatedSpeed : raw.speed;
    filtered.accuracy = raw.accuracy * 0.8f; // Assume better accuracy after filtering

    LOGD("Filtered location: %.6f, %.6f (from %.6f, %.6f), bearing: %.1f, speed: %.1f",
         filtered.latitude, filtered.longitude, raw.latitude, raw.longitude,
         filtered.bearing, filtered.speed);

    return filtered;
}