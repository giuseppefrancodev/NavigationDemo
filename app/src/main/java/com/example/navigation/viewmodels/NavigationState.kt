package com.example.navigation.viewmodels

import com.example.navigation.domain.models.Location
import com.example.navigation.domain.models.Route
import com.example.navigation.domain.models.RouteMatch

/**
 * Data class representing the current state of the navigation system.
 * This simpler version avoids the complexity of a sealed class hierarchy
 * while still providing all the necessary state information.
 */
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
) {
    /**
     * Calculated property that determines if location tracking is active.
     * This simplifies checks in the UI layer.
     */
    val isLocationTrackingActive: Boolean
        get() = currentLocation != null && isLocationTracking

    /**
     * Calculated property that determines if route preview should be shown.
     * This simplifies checks in the UI layer.
     */
    val shouldShowRoutePreview: Boolean
        get() = !isNavigating && currentRoute != null && alternativeRoutes.isNotEmpty()

    /**
     * Calculated property that determines if navigation panel should be shown.
     * This simplifies checks in the UI layer.
     */
    val shouldShowNavigationPanel: Boolean
        get() = isNavigating && currentRouteMatch != null
}