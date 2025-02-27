#include "route_matcher.h"
#include <android/log.h>

#define LOG_TAG "RouteMatcher"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

RouteMatcher::RouteMatcher(RoadGraph* graph) 
    : roadGraph(graph) {
    LOGI("RouteMatcher created");
}

RouteMatch RouteMatcher::match(const Location& loc) {
    LOGI("Matching location: %.6f, %.6f", loc.latitude, loc.longitude);

    // Store the last location
    lastLocation = loc;

    // Return a more complete test match to verify functionality
    RouteMatch match;
    match.streetName = "Main Street";
    match.nextManeuver = "Turn right in 200m";
    match.distanceToNext = 200;  // 200 meters
    match.estimatedTimeOfArrival = "10:30 AM";
    match.matchedLatitude = loc.latitude;
    match.matchedLongitude = loc.longitude;
    match.matchedBearing = loc.bearing;

    return match;
}

void RouteMatcher::setRoute(const Route& route) {
    LOGI("Setting route with %zu points", route.points.size());
    currentRoute = route;
}

Location RouteMatcher::getCurrentLocation() const {
    if (lastLocation.has_value()) {
        return lastLocation.value();
    }
    
    // Default location if none exists
    return Location{0, 0, 0, 0, 0};
}