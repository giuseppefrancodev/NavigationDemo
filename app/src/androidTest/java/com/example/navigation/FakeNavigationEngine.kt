package com.example.navigation

import com.example.navigation.domain.models.Location
import com.example.navigation.domain.models.Route
import com.example.navigation.domain.models.RouteMatch
import com.example.navigation.interfaces.NavigationEngineInterface

class FakeNavigationEngine : NavigationEngineInterface {
    private var shouldReturnSuccessForDestination = true
    private var shouldReturnSuccessForRouteSwitch = true
    private val alternativeRoutes = mutableListOf<Route>()

    fun setDestinationSuccess(success: Boolean) {
        shouldReturnSuccessForDestination = success
    }

    fun setAlternativeRoutes(routes: List<Route>) {
        alternativeRoutes.clear()
        alternativeRoutes.addAll(routes)
    }

    override fun updateLocation(
        latitude: Double,
        longitude: Double,
        bearing: Float,
        speed: Float,
        accuracy: Float
    ): RouteMatch {
        return RouteMatch(
            streetName = "Test Street",
            nextManeuver = "Continue Straight",
            distanceToNext = 100,
            estimatedTimeOfArrival = "10:00 AM",
            matchedLatitude = latitude,
            matchedLongitude = longitude,
            matchedBearing = bearing
        )
    }

    override fun setDestination(latitude: Double, longitude: Double): Boolean {
        return shouldReturnSuccessForDestination
    }

    override fun getAlternativeRoutes(): List<Route> {
        if (alternativeRoutes.isEmpty()) {
            return listOf(
                Route(
                    id = "test-route-1",
                    points = listOf(
                        Location(60.16, 24.93),
                        Location(60.17, 24.94)
                    ),
                    durationSeconds = 300,
                    name = "Test Route"
                )
            )
        }
        return alternativeRoutes
    }

    override fun switchToRoute(routeId: String): Boolean {
        return shouldReturnSuccessForRouteSwitch
    }

    override fun getDetailedPath(
        startLat: Double,
        startLon: Double,
        endLat: Double,
        endLon: Double,
        maxSegments: Int
    ): List<Location> {
        return listOf(
            Location(startLat, startLon),
            Location((startLat + endLat) / 2, (startLon + endLon) / 2),
            Location(endLat, endLon)
        )
    }

    override fun loadOSMDataFromAssets(assetFileName: String): Boolean {
        return true
    }
}
