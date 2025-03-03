/*
 * File: NavigationEngine.kt
 * Description: Bridge class between Kotlin and C++ native navigation engine. Provides methods to communicate with the native code.
 * Author: Giuseppe Franco
 * Created: March 2025
 */

package com.example.navigation

import android.content.Context
import com.example.navigation.domain.models.Location
import com.example.navigation.domain.models.Route
import com.example.navigation.domain.models.RouteMatch
import com.example.navigation.interfaces.NavigationEngineInterface

class NavigationEngine(private val context: Context) : NavigationEngineInterface {
    init {
        System.loadLibrary("navigation_engine")
        setContext(context)
    }

    /**
     * Updates the current location in the native navigation engine.
     * @return Updated route match information based on the new location.
     */
    external override fun updateLocation(
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
    external override fun setDestination(latitude: Double, longitude: Double): Boolean

    /**
     * Retrieves alternative routes for the current origin and destination.
     * @return List of possible routes.
     */
    external override fun getAlternativeRoutes(): List<Route>

    /**
     * Switches to a different calculated route.
     * @param routeId ID of the route to switch to.
     * @return True if switch was successful.
     */
    external override fun switchToRoute(routeId: String): Boolean

    /**
     * Gets a detailed path between two points that follows the road network.
     * This method uses the native road graph to generate a path that follows
     * actual roads, ensuring realistic navigation simulation.
     *
     * @param startLat Starting latitude
     * @param startLon Starting longitude
     * @param endLat Ending latitude
     * @param endLon Ending longitude
     * @param maxSegments Maximum number of segments to return (0 for unlimited)
     * @return List of locations representing a path that follows roads
     */
    external override fun getDetailedPath(
        startLat: Double,
        startLon: Double,
        endLat: Double,
        endLon: Double,
        maxSegments: Int
    ): List<Location>

    /**
     * Loads OpenStreetMap data from the assets directory.
     * This allows using real map data instead of the demo network.
     *
     * @param assetFileName Name of the OSM file in the assets directory (e.g., "finland.osm" or "helsinki.osm.pbf")
     * @return True if loading was successful
     */
    external override fun loadOSMDataFromAssets(assetFileName: String): Boolean

    private external fun setContext(context: Context)
}
