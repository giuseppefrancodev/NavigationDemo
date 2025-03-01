package com.example.navigation.utils

import android.util.Log
import com.example.navigation.NavigationEngine
import com.example.navigation.domain.models.Location
import com.example.navigation.domain.models.Route
import com.example.navigation.domain.models.RouteMatch
import kotlinx.coroutines.*
import java.util.UUID
import kotlin.math.max
import kotlin.math.min

private const val TAG = "NavigationSimulator"

/**
 * Utility class for simulating navigation by generating a series of location updates
 * that follow a path. Used for demo and testing purposes.
 */
class NavigationSimulator(
    private val navigationEngine: NavigationEngine,
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

    /**
     * Start a simulation using the provided route and callback.
     *
     * 1. If 'route' is null, we'll try to compute a real route from the NavigationEngine
     *    (this will follow the road graph).
     * 2. If that fails, we create a fallback "straight line" route as a last resort.
     */
    fun startSimulation(
        route: Route?,
        currentLocation: Location?,
        destination: Location?,
        callback: SimulationCallback
    ): Boolean {
        // Stop any existing simulation
        if (simulationJob?.isActive == true) {
            Log.w(TAG, "Simulation already running, stopping previous simulation")
            stopSimulation()
        }

        // Validate inputs
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
            // 1) If the caller didn't supply a route, compute one via the NavigationEngine
            val simulationRoute: Route = route ?: run {
                // -> Use setDestination to run the real road-based routing
                navigationEngine.setDestination(destination.latitude, destination.longitude)

                // -> Grab the computed alternatives
                val computedRoutes = navigationEngine.getAlternativeRoutes()
                if (computedRoutes.isNotEmpty()) {
                    // We'll pick the first route as the "best" or "primary"
                    computedRoutes[0]
                } else {
                    // Fallback: if no routes found from the engine, do a linear route
                    Log.w(TAG, "No real routes found - using fallback linear path")

                    val fallbackPath = createStraightLinePath(currentLocation, destination)
                    createRouteFromPath(fallbackPath)
                }
            }

            // 2) Start simulation using that route
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

    /**
     * Stop the current simulation if one is running.
     */
    fun stopSimulation() {
        simulationJob?.cancel()
        simulationJob = null
        currentCallback = null
        Log.d(TAG, "Simulation stopped")
    }

    /**
     * Core simulation function that interpolates between the route's points to create
     * a smooth navigation experience.
     */
    private suspend fun simulateNavigation(
        path: List<Location>,
        route: Route,
        callback: SimulationCallback
    ) {
        if (path.isEmpty()) {
            callback.onSimulationError("Path is empty, cannot simulate navigation")
            return
        }

        try {
            // Speed factor for faster/slower simulation
            val speedFactor = 5.0

            // Go through each segment in the route
            for (i in 0 until path.size - 1) {
                val startPoint = path[i]
                val endPoint   = path[i + 1]

                // Distance & bearing between these points
                val distance = LocationUtils.calculateDistanceInMeters(
                    startPoint.latitude, startPoint.longitude,
                    endPoint.latitude, endPoint.longitude
                )
                val bearing = LocationUtils.calculateBearing(
                    startPoint.latitude, startPoint.longitude,
                    endPoint.latitude, endPoint.longitude
                )

                // Speed (m/s) - pick from the route's points or a default
                val speedMps = max(endPoint.speed, 10f)

                // Time to travel this segment
                val timeNeededSeconds = (distance / speedMps / speedFactor).toInt()
                val numSteps = max(10, timeNeededSeconds) // at least 10 steps

                Log.d(
                    TAG, "Segment $i: distance=${distance}m, bearing=$bearing°, " +
                            "time=${timeNeededSeconds}s, steps=$numSteps"
                )

                // Create intermediate updates
                for (step in 0..numSteps) {
                    if (!coroutineScope.isActive) {
                        Log.d(TAG, "Simulation cancelled during execution")
                        return
                    }

                    val ratio = step.toDouble() / numSteps
                    val interpolatedPoint = interpolateLocation(startPoint, endPoint, ratio, bearing)

                    // Push location to the engine
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

                    // ~30fps
                    delay(33L)
                }
            }

            // Done
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

    /**
     * Interpolate between two points in the route for smooth movement.
     * (Same logic you already have.)
     */
    private fun interpolateLocation(
        start: Location,
        end: Location,
        ratio: Double,
        bearing: Double
    ): Location {
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

    /**
     * If no road-based route is found, we create a simple linear path from current to destination.
     */
    private fun createStraightLinePath(start: Location, end: Location): List<Location> {
        val path = mutableListOf<Location>()

        // E.g. 30 points
        val segments = 30
        for (i in 0..segments) {
            val ratio = i.toDouble() / segments.toDouble()
            val lat = start.latitude + (end.latitude - start.latitude) * ratio
            val lon = start.longitude + (end.longitude - start.longitude) * ratio
            // Keep speed, etc. as desired
            path.add(Location(lat, lon, 0f, 10f, 5f))
        }
        return path
    }

    /**
     * Create a Route object from a path of locations (used for fallback or test).
     */
    private fun createRouteFromPath(path: List<Location>): Route {
        // Basic distance sum
        var totalDistance = 0.0
        for (i in 1 until path.size) {
            totalDistance += LocationUtils.calculateDistanceInMeters(
                path[i - 1].latitude, path[i - 1].longitude,
                path[i].latitude,     path[i].longitude
            )
        }
        // Example speed ~8.33m/s => ~30km/h
        val durationSeconds = (totalDistance / 8.33).toInt()

        return Route(
            id = "sim-${UUID.randomUUID()}",
            points = path,
            durationSeconds = durationSeconds,
            name = "Fallback Route"
        )
    }
}
