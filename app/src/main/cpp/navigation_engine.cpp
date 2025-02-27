#include "navigation_engine.h"
#include "route_matcher.h"
#include "location_filter.h"
#include "road_graph.h"
#include "routing_engine.h"
#include <android/log.h>
#include <jni.h>
#include <memory>
#include <stdexcept>
#include <vector>
#include <chrono>
#include <string>

#define LOG_TAG "NavigationEngine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Singleton instance
static std::unique_ptr<NavigationEngine> gNavigationEngine;

NavigationEngine::NavigationEngine() {
    LOGI("Creating NavigationEngine");

    try {
        roadGraph = std::make_unique<RoadGraph>();
        routingEngine = std::make_unique<RoutingEngine>(roadGraph.get());
        routeMatcher = std::make_unique<RouteMatcher>(roadGraph.get());
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
    // Filter raw location data
    Location rawLocation{lat, lon, bearing, speed, accuracy};
    LOGI("Processing location: lat=%.6f, lon=%.6f, bearing=%.1f, speed=%.1f, accuracy=%.1f",
         lat, lon, bearing, speed, accuracy);

    // Apply Kalman filtering
    Location filtered = locationFilter->process(rawLocation);

    // Update current location
    currentLocation = filtered;

    // If we have a destination but no routes yet, calculate routes now
    if (destinationLocation.has_value() && alternativeRoutes.empty()) {
        LOGI("We have current location now, calculating routes to saved destination");
        alternativeRoutes = routingEngine->calculateRoutes(
                currentLocation.value(),
                destinationLocation.value()
        );

        if (!alternativeRoutes.empty()) {
            LOGI("Calculated %zu alternative routes", alternativeRoutes.size());
            // Set the first route as current
            currentRoute = alternativeRoutes[0];
            routeMatcher->setRoute(alternativeRoutes[0]);
        }
    }

    // Match location to route if we have one
    if (currentRoute) {
        return routeMatcher->match(filtered);
    }

    // Return empty match if no route is active
    RouteMatch emptyMatch;
    emptyMatch.streetName = "No active route";
    emptyMatch.nextManeuver = "Set a destination";
    emptyMatch.distanceToNext = 0;
    emptyMatch.estimatedTimeOfArrival = "";
    emptyMatch.matchedLatitude = filtered.latitude;
    emptyMatch.matchedLongitude = filtered.longitude;
    emptyMatch.matchedBearing = filtered.bearing;
    return emptyMatch;
}

bool NavigationEngine::setDestination(double lat, double lon) {
    LOGI("Setting destination: lat=%.6f, lon=%.6f", lat, lon);

    // Store the destination even if we don't have current location yet
    destinationLocation = Location{lat, lon};

    // If we don't have current location, just store the destination and return success
    if (!currentLocation.has_value()) {
        LOGI("Destination set, but waiting for current location before calculating routes");
        return true;
    }

    // Calculate routes
    alternativeRoutes = routingEngine->calculateRoutes(
            currentLocation.value(),
            destinationLocation.value()
    );

    if (alternativeRoutes.empty()) {
        LOGE("Failed to calculate any routes");
        return false;
    }

    LOGI("Calculated %zu alternative routes", alternativeRoutes.size());
    return true;
}

std::vector<Route> NavigationEngine::getAlternativeRoutes() const {
    return alternativeRoutes;
}

bool NavigationEngine::switchToRoute(const std::string& routeId) {
    LOGI("Switching to route %s", routeId.c_str());

    for (const auto& route : alternativeRoutes) {
        if (route.id == routeId) {
            currentRoute = route;
            routeMatcher->setRoute(route);
            LOGI("Route switched successfully");
            return true;
        }
    }

    LOGE("Route %s not found", routeId.c_str());
    return false;
}

// Helper function to convert C++ structs to Java objects
jobject createRouteMatchObject(JNIEnv* env, const RouteMatch& match) {
    // Find the RouteMatch class
    jclass routeMatchClass = env->FindClass("com/example/navigation/domain/models/RouteMatch");
    if (routeMatchClass == nullptr) {
        LOGE("Failed to find RouteMatch class");
        return nullptr;
    }

    // Find the constructor
    jmethodID constructor = env->GetMethodID(routeMatchClass, "<init>",
                                             "(Ljava/lang/String;Ljava/lang/String;ILjava/lang/String;DDF)V");
    if (constructor == nullptr) {
        LOGE("Failed to find RouteMatch constructor");
        // Add more detailed error logging
        jthrowable exception = env->ExceptionOccurred();
        if (exception) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        env->DeleteLocalRef(routeMatchClass);
        return nullptr;
    }

    // Create Java strings
    jstring streetName = env->NewStringUTF(match.streetName.c_str());
    jstring nextManeuver = env->NewStringUTF(match.nextManeuver.c_str());
    jstring eta = env->NewStringUTF(match.estimatedTimeOfArrival.c_str());

    // Create the RouteMatch object
    jobject result = env->NewObject(routeMatchClass, constructor,
                                    streetName, nextManeuver, match.distanceToNext, eta,
                                    match.matchedLatitude, match.matchedLongitude, match.matchedBearing);

    // Clean up local references
    env->DeleteLocalRef(streetName);
    env->DeleteLocalRef(nextManeuver);
    env->DeleteLocalRef(eta);
    env->DeleteLocalRef(routeMatchClass);

    return result;
}

// JNI method implementations
extern "C" JNIEXPORT jobject JNICALL
Java_com_example_navigation_NavigationEngine_updateLocation(
        JNIEnv* env, jobject thiz, jdouble lat, jdouble lon,
        jfloat bearing, jfloat speed, jfloat accuracy) {

    jobject result = nullptr;

    try {
        // Log incoming parameters
        LOGI("updateLocation called with: lat=%.6f, lon=%.6f, bearing=%.1f, speed=%.1f, accuracy=%.1f",
             lat, lon, bearing, speed, accuracy);

        // Create engine if it doesn't exist
        if (!gNavigationEngine) {
            LOGI("Creating NavigationEngine instance");
            gNavigationEngine = std::make_unique<NavigationEngine>();
        }

        // Update location
        RouteMatch match = gNavigationEngine->updateLocation(
                lat, lon, bearing, speed, accuracy);

        // Convert to Java object
        result = createRouteMatchObject(env, match);
        if (result == nullptr) {
            LOGE("Failed to create RouteMatch object");
            // Throw a more specific exception
            jclass exceptionClass = env->FindClass("java/lang/IllegalStateException");
            env->ThrowNew(exceptionClass, "Failed to create RouteMatch object");
            env->DeleteLocalRef(exceptionClass);
        }
    } catch (const std::exception& e) {
        LOGE("Error in updateLocation: %s", e.what());
        jclass exceptionClass = env->FindClass("java/lang/RuntimeException");
        env->ThrowNew(exceptionClass, e.what());
        env->DeleteLocalRef(exceptionClass);
    } catch (...) {
        // Catch all other exceptions
        LOGE("Unknown error in updateLocation");
        jclass exceptionClass = env->FindClass("java/lang/RuntimeException");
        env->ThrowNew(exceptionClass, "Unknown error in native code");
        env->DeleteLocalRef(exceptionClass);
    }

    return result;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_navigation_NavigationEngine_setDestination(
        JNIEnv* env, jobject thiz, jdouble lat, jdouble lon) {

    try {
        // Create engine if it doesn't exist
        if (!gNavigationEngine) {
            gNavigationEngine = std::make_unique<NavigationEngine>();
        }

        // Set destination
        return gNavigationEngine->setDestination(lat, lon);
    } catch (const std::exception& e) {
        LOGE("Error in setDestination: %s", e.what());
        jclass exceptionClass = env->FindClass("java/lang/RuntimeException");
        env->ThrowNew(exceptionClass, e.what());
        env->DeleteLocalRef(exceptionClass);
        return false;
    }
}

// Helper function to convert C++ Route to Java Route object
jobject createRouteObject(JNIEnv* env, const Route& route) {
    // Find the Route class
    jclass routeClass = env->FindClass("com/example/navigation/domain/models/Route");
    if (routeClass == nullptr) {
        LOGE("Failed to find Route class");
        return nullptr;
    }

    // Find the Location class
    jclass locationClass = env->FindClass("com/example/navigation/domain/models/Location");
    if (locationClass == nullptr) {
        LOGE("Failed to find Location class");
        env->DeleteLocalRef(routeClass);
        return nullptr;
    }

    // Find Location constructor
    jmethodID locationConstructor = env->GetMethodID(locationClass, "<init>",
                                                     "(DDFFF)V");
    if (locationConstructor == nullptr) {
        LOGE("Failed to find Location constructor");
        env->DeleteLocalRef(locationClass);
        env->DeleteLocalRef(routeClass);
        return nullptr;
    }

    // Create the points ArrayList
    jclass arrayListClass = env->FindClass("java/util/ArrayList");
    jmethodID arrayListConstructor = env->GetMethodID(arrayListClass, "<init>", "()V");
    jmethodID arrayListAdd = env->GetMethodID(arrayListClass, "add", "(Ljava/lang/Object;)Z");

    jobject pointsList = env->NewObject(arrayListClass, arrayListConstructor);

    // Add points to the list
    for (const auto& point : route.points) {
        jobject locationObject = env->NewObject(locationClass, locationConstructor,
                                                point.latitude, point.longitude, point.bearing, point.speed, point.accuracy);
        env->CallBooleanMethod(pointsList, arrayListAdd, locationObject);
        env->DeleteLocalRef(locationObject);
    }

    // Find Route constructor
    jmethodID routeConstructor = env->GetMethodID(routeClass, "<init>",
                                                  "(Ljava/lang/String;Ljava/util/List;ILjava/lang/String;)V");
    if (routeConstructor == nullptr) {
        LOGE("Failed to find Route constructor");
        env->DeleteLocalRef(pointsList);
        env->DeleteLocalRef(arrayListClass);
        env->DeleteLocalRef(locationClass);
        env->DeleteLocalRef(routeClass);
        return nullptr;
    }

    // Create Java strings
    jstring id = env->NewStringUTF(route.id.c_str());
    jstring name = env->NewStringUTF(route.name.c_str());

    // Create the Route object
    jobject result = env->NewObject(routeClass, routeConstructor,
                                    id, pointsList, route.durationSeconds, name);

    // Clean up local references
    env->DeleteLocalRef(id);
    env->DeleteLocalRef(name);
    env->DeleteLocalRef(pointsList);
    env->DeleteLocalRef(arrayListClass);
    env->DeleteLocalRef(locationClass);
    env->DeleteLocalRef(routeClass);

    return result;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_example_navigation_NavigationEngine_getAlternativeRoutes(
        JNIEnv* env, jobject thiz) {

    try {
        // Create engine if it doesn't exist
        if (!gNavigationEngine) {
            gNavigationEngine = std::make_unique<NavigationEngine>();
        }

        // Get routes
        std::vector<Route> routes = gNavigationEngine->getAlternativeRoutes();

        // Create the ArrayList to return
        jclass arrayListClass = env->FindClass("java/util/ArrayList");
        jmethodID arrayListConstructor = env->GetMethodID(arrayListClass, "<init>", "()V");
        jmethodID arrayListAdd = env->GetMethodID(arrayListClass, "add", "(Ljava/lang/Object;)Z");

        jobject routesList = env->NewObject(arrayListClass, arrayListConstructor);

        // Add routes to the list
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
        jclass exceptionClass = env->FindClass("java/lang/RuntimeException");
        env->ThrowNew(exceptionClass, e.what());
        env->DeleteLocalRef(exceptionClass);
        return nullptr;
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_navigation_NavigationEngine_switchToRoute(
        JNIEnv* env, jobject thiz, jstring routeId) {

    try {
        // Create engine if it doesn't exist
        if (!gNavigationEngine) {
            gNavigationEngine = std::make_unique<NavigationEngine>();
        }

        // Convert Java string to C++ string
        const char* idChars = env->GetStringUTFChars(routeId, nullptr);
        std::string id(idChars);
        env->ReleaseStringUTFChars(routeId, idChars);

        // Switch route
        return gNavigationEngine->switchToRoute(id);
    } catch (const std::exception& e) {
        LOGE("Error in switchToRoute: %s", e.what());
        jclass exceptionClass = env->FindClass("java/lang/RuntimeException");
        env->ThrowNew(exceptionClass, e.what());
        env->DeleteLocalRef(exceptionClass);
        return false;
    }
}