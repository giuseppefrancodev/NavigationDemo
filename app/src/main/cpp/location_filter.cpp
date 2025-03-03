/*
 * File: location_filter.cpp
 * Description: Implementation of the LocationFilter class, responsible for filtering and smoothing raw location data.
 * Author: Giuseppe Franco
 * Created: March 2025
 */

#include "location_filter.h"
#include <android/log.h>
#include <cmath>

#define LOG_TAG "LocationFilter"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

constexpr double INITIAL_POSITION_VARIANCE = 10.0;
constexpr double INITIAL_VELOCITY_VARIANCE = 5.0;
constexpr double PROCESS_NOISE_POSITION    = 0.01;
constexpr double PROCESS_NOISE_VELOCITY    = 0.1;
constexpr double BASE_MEASUREMENT_NOISE    = 5.0;

LocationFilter::LocationFilter() {
    LOGI("LocationFilter created");

    lat     = 0.0;
    lon     = 0.0;
    lat_vel = 0.0;
    lon_vel = 0.0;

    positionVariance    = INITIAL_POSITION_VARIANCE;
    velocityVariance    = INITIAL_VELOCITY_VARIANCE;
    processNoisePosition= PROCESS_NOISE_POSITION;
    processNoiseVelocity= PROCESS_NOISE_VELOCITY;

    lastTimestamp       = 0;
}

Location LocationFilter::process(const Location& raw) {
    auto now   = std::chrono::system_clock::now();
    auto nowMs = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    long long currentTimestamp = nowMs.time_since_epoch().count();

    if (!initialized) {

        lat  = raw.latitude;
        lon  = raw.longitude;
        lat_vel = 0.0;
        lon_vel = 0.0;
        initialized = true;
        lastTimestamp = currentTimestamp;

        LOGI("Filter initialized with location: %.6f, %.6f", lat, lon);
        return raw;
    }

    double dt = (currentTimestamp - lastTimestamp) / 1000.0;
    if (dt <= 0 || dt > 10.0) {
        LOGI("Invalid time delta: %.3f seconds, resetting to 0.1", dt);
        dt = 0.1;
    }
    lastTimestamp = currentTimestamp;

    double adaptedNoise = BASE_MEASUREMENT_NOISE;
    if (raw.accuracy > 0) {
        adaptedNoise = BASE_MEASUREMENT_NOISE * (raw.accuracy / 10.0);
    }

    double predictedLat = lat + lat_vel * dt;
    double predictedLon = lon + lon_vel * dt;

    double predictedPosVar = positionVariance + processNoisePosition + velocityVariance * dt * dt;
    double predictedVelVar = velocityVariance + processNoiseVelocity;

    double kLat = predictedPosVar / (predictedPosVar + adaptedNoise);
    double kLon = predictedPosVar / (predictedPosVar + adaptedNoise);

    kLat = std::max(0.1, std::min(kLat, 0.9));
    kLon = std::max(0.1, std::min(kLon, 0.9));

    lat = predictedLat + kLat * (raw.latitude - predictedLat);
    lon = predictedLon + kLon * (raw.longitude - predictedLon);

    double new_lat_vel = (raw.latitude - predictedLat) / dt;
    double new_lon_vel = (raw.longitude - predictedLon) / dt;

    const double MAX_VELOCITY_CHANGE = 10.0;
    if (std::abs(new_lat_vel - lat_vel) > MAX_VELOCITY_CHANGE) {
        new_lat_vel = lat_vel + std::copysign(MAX_VELOCITY_CHANGE, new_lat_vel - lat_vel);
    }
    if (std::abs(new_lon_vel - lon_vel) > MAX_VELOCITY_CHANGE) {
        new_lon_vel = lon_vel + std::copysign(MAX_VELOCITY_CHANGE, new_lon_vel - lon_vel);
    }

    lat_vel = lat_vel * 0.7 + new_lat_vel * 0.3;
    lon_vel = lon_vel * 0.7 + new_lon_vel * 0.3;

    positionVariance = (1 - kLat) * predictedPosVar;
    velocityVariance = (1 - kLat) * predictedVelVar;

    float calculatedBearing = raw.bearing;
    float calculatedSpeed   = raw.speed;

    double velocityMagnitude = std::sqrt(lat_vel * lat_vel + lon_vel * lon_vel);
    if (velocityMagnitude > 0.00001) {
        calculatedBearing = static_cast<float>(std::atan2(lon_vel, lat_vel) * 180.0 / M_PI);
        if (calculatedBearing < 0) {
            calculatedBearing += 360.0f;
        }

        calculatedSpeed = static_cast<float>(velocityMagnitude * 111000);
    }

    Location filtered;
    filtered.latitude  = lat;
    filtered.longitude = lon;

    filtered.bearing   = std::isnan(raw.bearing) ? calculatedBearing : raw.bearing;
    filtered.speed     = std::isnan(raw.speed)   ? calculatedSpeed   : raw.speed;
    filtered.accuracy  = raw.accuracy * 0.8f;

    LOGD("Filtered location: %.6f, %.6f (from %.6f, %.6f), bearing: %.1f, speed: %.1f",
         filtered.latitude, filtered.longitude, raw.latitude, raw.longitude,
         filtered.bearing, filtered.speed);

    return filtered;
}
