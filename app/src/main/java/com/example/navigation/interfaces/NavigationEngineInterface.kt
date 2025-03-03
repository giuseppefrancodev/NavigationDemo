package com.example.navigation.interfaces

import com.example.navigation.domain.models.Location
import com.example.navigation.domain.models.Route
import com.example.navigation.domain.models.RouteMatch

interface NavigationEngineInterface {
    fun updateLocation(latitude: Double, longitude: Double, bearing: Float, speed: Float, accuracy: Float): RouteMatch

    fun setDestination(latitude: Double, longitude: Double): Boolean

    fun getAlternativeRoutes(): List<Route>

    fun switchToRoute(routeId: String): Boolean

    fun getDetailedPath(
        startLat: Double,
        startLon: Double,
        endLat: Double,
        endLon: Double,
        maxSegments: Int
    ): List<Location>

    fun loadOSMDataFromAssets(assetFileName: String): Boolean
}
