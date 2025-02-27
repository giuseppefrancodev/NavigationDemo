package com.example.navigation

import com.example.navigation.domain.models.RouteMatch
import com.example.navigation.domain.models.Route

/**
 * Bridge class between Kotlin and C++ native navigation engine.
 * Provides methods to communicate with the native code.
 */
class NavigationEngine {
    init {
        System.loadLibrary("navigation_engine")
    }

    /**
     * Updates the current location in the native navigation engine.
     * @return Updated route match information based on the new location.
     */
    external fun updateLocation(
        latitude: Double,
        longitude: Double,
        bearing: Float,
        speed: Float,
        accuracy: Float
    ): RouteMatch

    /**
     * Sets a destination for navigation.
     * @return True if route calculation was successful.
     */
    external fun setDestination(
        latitude: Double,
        longitude: Double
    ): Boolean

    /**
     * Retrieves alternative routes for the current origin and destination.
     * @return List of possible routes.
     */
    external fun getAlternativeRoutes(): List<Route>

    /**
     * Switches to a different calculated route.
     * @param routeId ID of the route to switch to.
     * @return True if switch was successful.
     */
    external fun switchToRoute(routeId: String): Boolean
}