/*
 * File: NavigationSimulator.kt
 * Description: Simulator class for testing navigation functionality by simulating location updates and route matching.
 * Author: Giuseppe Franco
 * Created: March 2025
 */

package com.example.navigation.utils

import android.util.Log
import com.example.navigation.domain.models.Location
import com.example.navigation.domain.models.Route
import com.example.navigation.domain.models.RouteMatch
import com.example.navigation.interfaces.NavigationEngineInterface
import java.util.UUID
import kotlin.math.max
import kotlinx.coroutines.*

private const val TAG = "NavigationSimulator"

class NavigationSimulator(
    private val navigationEngine: NavigationEngineInterface,
    private val coroutineScope: CoroutineScope
) {
    interface SimulationCallback {
        fun onLocationUpdated(location: Location)
        fun onRouteMatchUpdated(routeMatch: RouteMatch)
        fun onSimulationCompleted()
        fun onSimulationError(message: String)
        fun getEstimatedArrivalTime(route: Route?): String
    }

    private var simulationJob: Job? = null
    private var currentCallback: SimulationCallback? = null

    fun startSimulation(
        route: Route?,
        currentLocation: Location?,
        destination: Location?,
        callback: SimulationCallback
    ): Boolean {
        if (simulationJob?.isActive == true) {
            Log.w(TAG, "Simulation already running, stopping previous simulation")
            stopSimulation()
        }

        if (currentLocation == null) {
            Log.e(TAG, "Cannot start simulation - current location not available")
            return false
        }
        if (destination == null) {
            Log.e(TAG, "Cannot start simulation - destination not set")
            return false
        }

        currentCallback = callback

        try {
            val simulationRoute: Route = route ?: run {
                navigationEngine.setDestination(destination.latitude, destination.longitude)

                val computedRoutes = navigationEngine.getAlternativeRoutes()
                if (computedRoutes.isNotEmpty()) {
                    computedRoutes[0]
                } else {
                    Log.w(TAG, "No real routes found - using fallback linear path")

                    val fallbackPath = createStraightLinePath(currentLocation, destination)
                    createRouteFromPath(fallbackPath)
                }
            }

            simulationJob = coroutineScope.launch {
                simulateNavigation(simulationRoute.points, simulationRoute, callback)
            }

            return true
        } catch (e: Exception) {
            Log.e(TAG, "Error starting simulation", e)
            callback.onSimulationError("Failed to start simulation: ${e.message}")
            return false
        }
    }

    fun stopSimulation() {
        simulationJob?.cancel()
        simulationJob = null
        currentCallback = null
        Log.d(TAG, "Simulation stopped")
    }

    private suspend fun simulateNavigation(path: List<Location>, route: Route, callback: SimulationCallback) {
        if (path.isEmpty()) {
            callback.onSimulationError("Path is empty, cannot simulate navigation")
            return
        }

        try {
            val speedFactor = 5.0

            for (i in 0 until path.size - 1) {
                val startPoint = path[i]
                val endPoint = path[i + 1]

                val distance = LocationUtils.calculateDistanceInMeters(
                    startPoint.latitude,
                    startPoint.longitude,
                    endPoint.latitude,
                    endPoint.longitude
                )
                val bearing = LocationUtils.calculateBearing(
                    startPoint.latitude,
                    startPoint.longitude,
                    endPoint.latitude,
                    endPoint.longitude
                )

                val speedMps = max(endPoint.speed, 10f)

                val timeNeededSeconds = (distance / speedMps / speedFactor).toInt()
                val numSteps = max(10, timeNeededSeconds)

                Log.d(
                    TAG,
                    "Segment $i: distance=${distance}m, bearing=$bearing°, " +
                        "time=${timeNeededSeconds}s, steps=$numSteps"
                )

                for (step in 0..numSteps) {
                    if (!coroutineScope.isActive) {
                        Log.d(TAG, "Simulation cancelled during execution")
                        return
                    }

                    val ratio = step.toDouble() / numSteps
                    val interpolatedPoint = interpolateLocation(startPoint, endPoint, ratio, bearing)

                    val routeMatch = navigationEngine.updateLocation(
                        interpolatedPoint.latitude,
                        interpolatedPoint.longitude,
                        interpolatedPoint.bearing,
                        interpolatedPoint.speed,
                        interpolatedPoint.accuracy
                    )

                    val updatedRouteMatch = routeMatch.copy(
                        estimatedTimeOfArrival = callback.getEstimatedArrivalTime(route)
                    )

                    callback.onLocationUpdated(interpolatedPoint)
                    callback.onRouteMatchUpdated(updatedRouteMatch)

                    delay(33L)
                }
            }

            Log.d(TAG, "Simulation completed successfully")
            callback.onSimulationCompleted()
        } catch (e: Exception) {
            if (e is CancellationException) {
                Log.d(TAG, "Simulation cancelled")
            } else {
                Log.e(TAG, "Error during simulation", e)
                callback.onSimulationError("Simulation error: ${e.message}")
            }
        }
    }

    private fun interpolateLocation(start: Location, end: Location, ratio: Double, bearing: Double): Location {
        val lat = start.latitude + (end.latitude - start.latitude) * ratio
        val lon = start.longitude + (end.longitude - start.longitude) * ratio

        val speed = start.speed + (end.speed - start.speed) * ratio.toFloat()
        val finalBearing = if (ratio > 0.98) end.bearing else bearing.toFloat()

        return Location(
            latitude = lat,
            longitude = lon,
            bearing = finalBearing,
            speed = speed,
            accuracy = 5f
        )
    }

    private fun createStraightLinePath(start: Location, end: Location): List<Location> {
        val path = mutableListOf<Location>()

        val segments = 30
        for (i in 0..segments) {
            val ratio = i.toDouble() / segments.toDouble()
            val lat = start.latitude + (end.latitude - start.latitude) * ratio
            val lon = start.longitude + (end.longitude - start.longitude) * ratio

            path.add(Location(lat, lon, 0f, 10f, 5f))
        }
        return path
    }

    private fun createRouteFromPath(path: List<Location>): Route {
        var totalDistance = 0.0
        for (i in 1 until path.size) {
            totalDistance += LocationUtils.calculateDistanceInMeters(
                path[i - 1].latitude, path[i - 1].longitude,
                path[i].latitude, path[i].longitude
            )
        }

        val durationSeconds = (totalDistance / 8.33).toInt()

        return Route(
            id = "sim-${UUID.randomUUID()}",
            points = path,
            durationSeconds = durationSeconds,
            name = "Fallback Route"
        )
    }
}
