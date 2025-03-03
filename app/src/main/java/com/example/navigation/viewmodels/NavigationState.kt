/*
 * File: NavigationSimulator.kt
 * Description: Simulator class for testing navigation functionality by simulating location updates and route matching.
 * Author: Giuseppe Franco
 * Created: March 2025
 */

package com.example.navigation.viewmodels

import com.example.navigation.domain.models.Location
import com.example.navigation.domain.models.Route
import com.example.navigation.domain.models.RouteMatch

data class NavigationState(
    val isNavigating: Boolean = false,
    val hasLocationPermission: Boolean = false,
    val currentLocation: Location? = null,
    val destination: Location? = null,
    val currentRoute: Route? = null,
    val alternativeRoutes: List<Route> = emptyList(),
    val currentRouteMatch: RouteMatch? = null,
    val isLocationTracking: Boolean = false,
    val errorMessage: String? = null,
    val isLoading: Boolean = false,
    val shouldUpdateCamera: Boolean = false,
    val zoomToRoute: Boolean = false
)
