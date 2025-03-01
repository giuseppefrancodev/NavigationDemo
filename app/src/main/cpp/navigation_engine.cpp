#include "navigation_engine.h"
#include <android/log.h>
#include <jni.h>
#include <memory>
#include <stdexcept>
#include <vector>
#include <chrono>
#include <string>
#include <cmath>
#include <algorithm> // for std::clamp (C++17+)

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define LOG_TAG "NavigationEngine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Global instance for JNI usage
static std::unique_ptr<NavigationEngine> gNavigationEngine;

double NavigationEngine::haversineDistance(double lat1, double lon1, double lat2, double lon2) {
    constexpr double R = 6371000.0; // Earth radius in meters
    double phi1 = lat1 * M_PI / 180.0;
    double phi2 = lat2 * M_PI / 180.0;
    double dPhi = (lat2 - lat1) * M_PI / 180.0;
    double dLambda = (lon2 - lon1) * M_PI / 180.0;

    double a = std::sin(dPhi/2) * std::sin(dPhi/2) +
               std::cos(phi1)  * std::cos(phi2) *
               std::sin(dLambda/2) * std::sin(dLambda/2);

    return R * 2.0 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
}

NavigationEngine::NavigationEngine() {
    LOGI("Creating NavigationEngine");
    try {
        roadGraph      = std::make_unique<RoadGraph>();
        routingEngine  = std::make_unique<RoutingEngine>(roadGraph.get());
        routeMatcher   = std::make_unique<RouteMatcher>(roadGraph.get());
        locationFilter = std::make_unique<LocationFilter>();
        LOGI("NavigationEngine created successfully");
    } catch (const std::exception& e) {
        LOGE("Error creating NavigationEngine: %s", e.what());
        throw;
    }
}

NavigationEngine::~NavigationEngine() {
    LOGI("Destroying NavigationEngine");
}

RouteMatch NavigationEngine::updateLocation(double lat, double lon, float bearing,
                                            float speed, float accuracy) {
    Location rawLocation{ lat, lon, bearing, speed, accuracy };
    LOGI("Processing location: lat=%.6f, lon=%.6f, bearing=%.1f, speed=%.1f, accuracy=%.1f",
         lat, lon, bearing, speed, accuracy);

    // Filter raw location data
    Location filtered = locationFilter->process(rawLocation);
    currentLocation = filtered;

    // If there's a destination and no routes yet, compute routes
    if (destinationLocation.has_value() && alternativeRoutes.empty()) {
        LOGI("Calculating routes to saved destination");
        alternativeRoutes = routingEngine->calculateRoutes(
                currentLocation.value(),
                destinationLocation.value()
        );
        if (!alternativeRoutes.empty()) {
            LOGI("Calculated %zu alternative routes", alternativeRoutes.size());
            currentRoute = alternativeRoutes[0];
            routeMatcher->setRoute(alternativeRoutes[0]);
        }
    }

    // If we have a route, do route matching
    if (currentRoute) {
        return routeMatcher->match(filtered);
    }

    // Otherwise, return placeholder
    return RouteMatch{
            .streetName             = "No active route",
            .nextManeuver           = "Set a destination",
            .distanceToNext         = 0,
            .estimatedTimeOfArrival = "",
            .matchedLatitude        = filtered.latitude,
            .matchedLongitude       = filtered.longitude,
            .matchedBearing         = filtered.bearing
    };
}

bool NavigationEngine::setDestination(double lat, double lon) {
    LOGI("Setting destination: lat=%.6f, lon=%.6f", lat, lon);
    destinationLocation = Location{ lat, lon, 0.0f, 0.0f, 0.0f };

    if (!currentLocation.has_value()) {
        LOGI("Waiting for current location before route calculation");
        return true;
    }

    alternativeRoutes = routingEngine->calculateRoutes(
            currentLocation.value(),
            destinationLocation.value()
    );

    if (alternativeRoutes.empty()) {
        LOGE("Failed to calculate routes");
        return false;
    }

    LOGI("Found %zu alternative routes", alternativeRoutes.size());
    return true;
}

std::vector<Route> NavigationEngine::getAlternativeRoutes() const {
    return alternativeRoutes;
}

bool NavigationEngine::switchToRoute(const std::string& routeId) {
    for (const auto& route : alternativeRoutes) {
        if (route.id == routeId) {
            currentRoute = route;
            routeMatcher->setRoute(route);
            LOGI("Switched to route %s", routeId.c_str());
            return true;
        }
    }
    LOGE("Route %s not found", routeId.c_str());
    return false;
}

void NavigationEngine::calculateBearingAndSpeed(std::vector<Location>& path) {
    if (path.size() < 2) return;

    for (size_t i = 0; i < path.size() - 1; i++) {
        double dLon = (path[i + 1].longitude - path[i].longitude) * M_PI / 180.0;
        double lat1 = path[i].latitude * M_PI / 180.0;
        double lat2 = path[i + 1].latitude * M_PI / 180.0;

        double y = std::sin(dLon) * std::cos(lat2);
        double x = std::cos(lat1) * std::sin(lat2)
                   - std::sin(lat1) * std::cos(lat2) * std::cos(dLon);

        float bearing = static_cast<float>(std::atan2(y, x) * 180.0 / M_PI);
        if (bearing < 0.0f) bearing += 360.0f;
        path[i].bearing = bearing;

        // Estimate speed by naive distance / 60sec
        double distance = haversineDistance(
                path[i].latitude, path[i].longitude,
                path[i + 1].latitude, path[i + 1].longitude);
        float rawSpeed = static_cast<float>(distance / 60.0);

        // clamp speed to 5..20
        path[i].speed = std::clamp(rawSpeed, 5.0f, 20.0f);
    }

    // Last point
    if (path.size() > 1) {
        path.back().bearing = path[path.size() - 2].bearing;
        path.back().speed   = 0.0f;
    }
}

std::vector<Location> NavigationEngine::getDetailedPath(
        double startLat, double startLon,
        double endLat, double endLon,
        int maxSegments) {

    std::vector<Location> result;
    LOGI("Generating path from (%.6f,%.6f) to (%.6f,%.6f)",
         startLat, startLon, endLat, endLon);

    // Use RoutingEngine to calculate a route
    Location startLocation{ startLat, startLon, 0.0f, 0.0f }; // Default bearing and speed
    Location endLocation{ endLat, endLon, 0.0f, 0.0f };       // Default bearing and speed

    // Calculate routes using RoutingEngine
    std::vector<Route> routes = routingEngine->calculateRoutes(startLocation, endLocation);

    if (!routes.empty()) {
        // Use the first route (primary route) for the detailed path
        Route primaryRoute = routes[0];
        result = primaryRoute.points;

        // Calculate bearing and speed for the path
        calculateBearingAndSpeed(result);

        LOGI("Generated road-following path with %zu points", result.size());
    } else {
        LOGE("Failed to generate road-following path, falling back to straight-line path");

        // Fallback: Generate a straight-line path with interpolation
        const int numPoints = std::max(10, maxSegments);
        for (int i = 0; i < numPoints; i++) {
            double ratio = static_cast<double>(i) / (numPoints - 1);
            double lat = startLat + (endLat - startLat) * ratio;
            double lon = startLon + (endLon - startLon) * ratio;

            // Basic bearing calculation
            double dx = (endLon - startLon);
            double dy = (endLat - startLat);
            float bearing = static_cast<float>(std::atan2(dx, dy) * 180.0 / M_PI);
            if (bearing < 0) bearing += 360.0f;

            result.emplace_back(lat, lon, bearing, 10.0f);
        }

        if (!result.empty()) {
            result.back().speed = 0.0f;
        }

        LOGI("Created fallback straight-line path with %zu points", result.size());
    }

    return result;
}

// -------------- JNI Glue Code Below --------------

jobject createRouteMatchObject(JNIEnv* env, const RouteMatch& match) {
    jclass routeMatchClass = env->FindClass("com/example/navigation/domain/models/RouteMatch");
    if (!routeMatchClass) {
        LOGE("Failed to find RouteMatch class");
        return nullptr;
    }

    jmethodID constructor = env->GetMethodID(
            routeMatchClass, "<init>",
            "(Ljava/lang/String;Ljava/lang/String;ILjava/lang/String;DDF)V");
    if (!constructor) {
        LOGE("Failed to find RouteMatch constructor");
        jthrowable exception = env->ExceptionOccurred();
        if (exception) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        env->DeleteLocalRef(routeMatchClass);
        return nullptr;
    }

    jstring streetName   = env->NewStringUTF(match.streetName.c_str());
    jstring nextManeuver = env->NewStringUTF(match.nextManeuver.c_str());
    jstring eta          = env->NewStringUTF(match.estimatedTimeOfArrival.c_str());

    jobject resultObj = env->NewObject(
            routeMatchClass,
            constructor,
            streetName,
            nextManeuver,
            static_cast<jint>(match.distanceToNext),
            eta,
            static_cast<jdouble>(match.matchedLatitude),
            static_cast<jdouble>(match.matchedLongitude),
            static_cast<jfloat>(match.matchedBearing));

    env->DeleteLocalRef(streetName);
    env->DeleteLocalRef(nextManeuver);
    env->DeleteLocalRef(eta);
    env->DeleteLocalRef(routeMatchClass);
    return resultObj;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_example_navigation_NavigationEngine_updateLocation(
        JNIEnv* env, jobject /*thiz*/,
        jdouble lat, jdouble lon,
        jfloat bearing, jfloat speed, jfloat accuracy) {

    jobject result = nullptr;
    try {
        LOGI("updateLocation called: lat=%.6f, lon=%.6f, bearing=%.1f, speed=%.1f, accuracy=%.1f",
             lat, lon, bearing, speed, accuracy);

        if (!gNavigationEngine) {
            LOGI("Creating NavigationEngine instance");
            gNavigationEngine = std::make_unique<NavigationEngine>();
        }

        RouteMatch match = gNavigationEngine->updateLocation(lat, lon, bearing, speed, accuracy);
        result = createRouteMatchObject(env, match);
        if (!result) {
            LOGE("Failed to create RouteMatch object");
            jclass exClass = env->FindClass("java/lang/IllegalStateException");
            env->ThrowNew(exClass, "Failed to create RouteMatch object");
            env->DeleteLocalRef(exClass);
        }

    } catch (const std::exception& e) {
        LOGE("Error in updateLocation: %s", e.what());
        jclass exClass = env->FindClass("java/lang/RuntimeException");
        env->ThrowNew(exClass, e.what());
        env->DeleteLocalRef(exClass);
    } catch (...) {
        LOGE("Unknown error in updateLocation");
        jclass exClass = env->FindClass("java/lang/RuntimeException");
        env->ThrowNew(exClass, "Unknown error in native code");
        env->DeleteLocalRef(exClass);
    }

    return result;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_navigation_NavigationEngine_setDestination(
        JNIEnv* env, jobject /*thiz*/,
        jdouble lat, jdouble lon) {

    try {
        if (!gNavigationEngine) {
            gNavigationEngine = std::make_unique<NavigationEngine>();
        }
        bool success = gNavigationEngine->setDestination(lat, lon);
        return static_cast<jboolean>(success);

    } catch (const std::exception& e) {
        LOGE("Error in setDestination: %s", e.what());
        jclass exClass = env->FindClass("java/lang/RuntimeException");
        env->ThrowNew(exClass, e.what());
        env->DeleteLocalRef(exClass);
        return JNI_FALSE;
    }
}

jobject createRouteObject(JNIEnv* env, const Route& route) {
    jclass routeClass = env->FindClass("com/example/navigation/domain/models/Route");
    if (!routeClass) {
        LOGE("Failed to find Route class");
        return nullptr;
    }

    jclass locationClass = env->FindClass("com/example/navigation/domain/models/Location");
    if (!locationClass) {
        LOGE("Failed to find Location class");
        env->DeleteLocalRef(routeClass);
        return nullptr;
    }

    jmethodID locationConstructor = env->GetMethodID(locationClass, "<init>", "(DDFFF)V");
    if (!locationConstructor) {
        LOGE("Failed to find Location constructor");
        env->DeleteLocalRef(locationClass);
        env->DeleteLocalRef(routeClass);
        return nullptr;
    }

    jclass arrayListClass = env->FindClass("java/util/ArrayList");
    jmethodID arrayListCtor = env->GetMethodID(arrayListClass, "<init>", "()V");
    jmethodID arrayListAdd  = env->GetMethodID(arrayListClass, "add", "(Ljava/lang/Object;)Z");

    jobject pointsList = env->NewObject(arrayListClass, arrayListCtor);

    for (const auto& point : route.points) {
        jobject locationObject = env->NewObject(
                locationClass,
                locationConstructor,
                point.latitude, point.longitude,
                point.bearing, point.speed, point.accuracy);
        env->CallBooleanMethod(pointsList, arrayListAdd, locationObject);
        env->DeleteLocalRef(locationObject);
    }

    jmethodID routeCtor = env->GetMethodID(
            routeClass, "<init>", "(Ljava/lang/String;Ljava/util/List;ILjava/lang/String;)V");
    if (!routeCtor) {
        LOGE("Failed to find Route constructor");
        env->DeleteLocalRef(pointsList);
        env->DeleteLocalRef(arrayListClass);
        env->DeleteLocalRef(locationClass);
        env->DeleteLocalRef(routeClass);
        return nullptr;
    }

    jstring jId   = env->NewStringUTF(route.id.c_str());
    jstring jName = env->NewStringUTF(route.name.c_str());

    jobject routeObj = env->NewObject(
            routeClass, routeCtor,
            jId, pointsList,
            static_cast<jint>(route.durationSeconds),
            jName);

    env->DeleteLocalRef(jId);
    env->DeleteLocalRef(jName);
    env->DeleteLocalRef(pointsList);
    env->DeleteLocalRef(arrayListClass);
    env->DeleteLocalRef(locationClass);
    env->DeleteLocalRef(routeClass);
    return routeObj;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_example_navigation_NavigationEngine_getAlternativeRoutes(
        JNIEnv* env, jobject /*thiz*/) {

    try {
        if (!gNavigationEngine) {
            gNavigationEngine = std::make_unique<NavigationEngine>();
        }

        std::vector<Route> routes = gNavigationEngine->getAlternativeRoutes();
        jclass arrayListClass     = env->FindClass("java/util/ArrayList");
        jmethodID arrayListCtor   = env->GetMethodID(arrayListClass, "<init>", "()V");
        jmethodID arrayListAdd    = env->GetMethodID(arrayListClass, "add", "(Ljava/lang/Object;)Z");

        jobject routesList = env->NewObject(arrayListClass, arrayListCtor);

        for (const auto& route : routes) {
            jobject routeObject = createRouteObject(env, route);
            if (routeObject) {
                env->CallBooleanMethod(routesList, arrayListAdd, routeObject);
                env->DeleteLocalRef(routeObject);
            }
        }

        env->DeleteLocalRef(arrayListClass);
        return routesList;

    } catch (const std::exception& e) {
        LOGE("Error in getAlternativeRoutes: %s", e.what());
        jclass exClass = env->FindClass("java/lang/RuntimeException");
        env->ThrowNew(exClass, e.what());
        env->DeleteLocalRef(exClass);
        return nullptr;
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_navigation_NavigationEngine_switchToRoute(
        JNIEnv* env, jobject /*thiz*/,
        jstring routeId) {

    try {
        if (!gNavigationEngine) {
            gNavigationEngine = std::make_unique<NavigationEngine>();
        }

        const char* idChars = env->GetStringUTFChars(routeId, nullptr);
        std::string id(idChars ? idChars : "");
        env->ReleaseStringUTFChars(routeId, idChars);

        bool success = gNavigationEngine->switchToRoute(id);
        return static_cast<jboolean>(success);

    } catch (const std::exception& e) {
        LOGE("Error in switchToRoute: %s", e.what());
        jclass exClass = env->FindClass("java/lang/RuntimeException");
        env->ThrowNew(exClass, e.what());
        env->DeleteLocalRef(exClass);
        return JNI_FALSE;
    }
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_example_navigation_NavigationEngine_getDetailedPath(
        JNIEnv* env, jobject /*thiz*/,
        jdouble startLat, jdouble startLon,
        jdouble endLat, jdouble endLon,
        jint maxSegments) {

    try {
        if (!gNavigationEngine) {
            gNavigationEngine = std::make_unique<NavigationEngine>();
        }

        std::vector<Location> path = gNavigationEngine->getDetailedPath(
                startLat, startLon, endLat, endLon, maxSegments);

        jclass arrayListClass     = env->FindClass("java/util/ArrayList");
        jmethodID arrayListCtor   = env->GetMethodID(arrayListClass, "<init>", "()V");
        jmethodID arrayListAdd    = env->GetMethodID(arrayListClass, "add", "(Ljava/lang/Object;)Z");

        jobject resultList = env->NewObject(arrayListClass, arrayListCtor);

        jclass locationClass = env->FindClass("com/example/navigation/domain/models/Location");
        jmethodID locationCtor = env->GetMethodID(locationClass, "<init>", "(DDFFF)V");

        for (const auto& loc : path) {
            jobject locObject = env->NewObject(locationClass, locationCtor,
                                               loc.latitude, loc.longitude,
                                               loc.bearing, loc.speed, loc.accuracy);
            env->CallBooleanMethod(resultList, arrayListAdd, locObject);
            env->DeleteLocalRef(locObject);
        }

        env->DeleteLocalRef(locationClass);
        env->DeleteLocalRef(arrayListClass);
        return resultList;

    } catch (const std::exception& e) {
        LOGE("Error in getDetailedPath: %s", e.what());
        jclass exClass = env->FindClass("java/lang/RuntimeException");
        env->ThrowNew(exClass, e.what());
        env->DeleteLocalRef(exClass);
        return nullptr;
    }
}
