/*
 * File: NavigationViewModel.kt
 * Description: ViewModel class for managing navigation logic, including location updates, route calculations, and simulation.
 * Author: Giuseppe Franco
 * Created: March 2025
 */

package com.example.navigation.viewmodels

import android.util.Log
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.example.navigation.domain.models.Location
import com.example.navigation.domain.models.Route
import com.example.navigation.domain.models.RouteMatch
import com.example.navigation.interfaces.NavigationEngineInterface
import com.example.navigation.interfaces.NavigationRepositoryInterface
import com.example.navigation.utils.FormatUtils
import com.example.navigation.utils.NavigationSimulator
import dagger.hilt.android.lifecycle.HiltViewModel
import javax.inject.Inject
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.catch
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch

private const val TAG = "NavigationViewModel"

@HiltViewModel
class NavigationViewModel @Inject constructor(
    private val navigationRepository: NavigationRepositoryInterface,
    private val navigationEngine: NavigationEngineInterface
) : ViewModel() {
    private val _state = MutableStateFlow(NavigationState())
    val state: StateFlow<NavigationState> = _state.asStateFlow()

    private var locationJob: Job? = null

    private val _isDemoMode = MutableStateFlow(false)
    val isDemoMode: StateFlow<Boolean> = _isDemoMode.asStateFlow()

    private val navigationSimulator = NavigationSimulator(
        navigationEngine = navigationEngine,
        coroutineScope = viewModelScope
    )

    private val simulationCallback = object : NavigationSimulator.SimulationCallback {
        override fun onLocationUpdated(location: Location) {
            _state.update { it.copy(currentLocation = location) }
        }

        override fun onRouteMatchUpdated(routeMatch: RouteMatch) {
            _state.update { it.copy(currentRouteMatch = routeMatch) }
        }

        override fun onSimulationCompleted() {
            _state.update { it.copy(errorMessage = "Destination reached") }
            viewModelScope.launch {
                delay(3000)
                stopSimulation()
            }
        }

        override fun onSimulationError(message: String) {
            _state.update { it.copy(errorMessage = message) }
            stopSimulation()
        }

        override fun getEstimatedArrivalTime(route: Route?): String {
            return FormatUtils.calculateEstimatedArrival(route)
        }
    }

    fun startSimulation() {
        locationJob?.cancel()

        Log.d(TAG, "startSimulation() called, route exists: ${_state.value.currentRoute != null}")

        val currentRoute = _state.value.currentRoute
        val currentLocation = _state.value.currentLocation
        val destination = _state.value.destination

        _state.update {
            it.copy(
                isNavigating = true,
                errorMessage = null
            )
        }

        val success = navigationSimulator.startSimulation(
            route = currentRoute,
            currentLocation = currentLocation,
            destination = destination,
            callback = simulationCallback
        )

        if (success) {
            _isDemoMode.value = true
        } else {
            _state.update {
                it.copy(
                    isNavigating = false,
                    errorMessage = "Could not start simulation - missing route or location"
                )
            }
        }
    }

    fun stopSimulation() {
        _isDemoMode.value = false
        navigationSimulator.stopSimulation()

        _state.update {
            it.copy(
                isNavigating = false,
                currentRouteMatch = null
            )
        }
    }

    fun onLocationPermissionGranted(hasPermission: Boolean) {
        _state.update {
            it.copy(
                hasLocationPermission = hasPermission,
                isLocationTracking = hasPermission,
                errorMessage = if (!hasPermission) "Location permission denied" else null
            )
        }
        if (hasPermission) {
            startLocationUpdates()
        }
    }

    private fun startLocationUpdates() {
        locationJob?.cancel()
        locationJob = viewModelScope.launch {
            try {
                _state.update { it.copy(isLocationTracking = true) }

                navigationRepository.getLocationUpdates()
                    .catch { e ->
                        Log.e(TAG, "Error in location updates", e)
                        _state.update {
                            it.copy(
                                errorMessage = "Location error: ${e.message}",
                                isLocationTracking = false
                            )
                        }

                        delay(1000)
                        startLocationUpdates()
                    }
                    .collect { location ->
                        processNewLocation(location)
                    }
            } catch (e: Exception) {
                Log.e(TAG, "Failed to start location updates", e)
                _state.update {
                    it.copy(
                        errorMessage = "Failed to get location: ${e.message}",
                        isLocationTracking = false
                    )
                }

                delay(1000)
                startLocationUpdates()
            }
        }
    }

    private fun processNewLocation(location: Location) {
        if (_isDemoMode.value) {
            return
        }

        val currentLocation = _state.value.currentLocation
        if (currentLocation != null &&
            currentLocation.latitude == location.latitude &&
            currentLocation.longitude == location.longitude
        ) {
            return
        }

        try {
            val shouldUpdateCamera = !_isDemoMode.value

            _state.update {
                it.copy(
                    currentLocation = location,
                    shouldUpdateCamera = shouldUpdateCamera
                )
            }

            val routeMatch = navigationEngine.updateLocation(
                location.latitude,
                location.longitude,
                location.bearing,
                location.speed,
                location.accuracy
            )

            if (_state.value.isNavigating) {
                _state.update {
                    it.copy(
                        currentRouteMatch = routeMatch.copy(
                            estimatedTimeOfArrival = routeMatch.estimatedTimeOfArrival.ifBlank {
                                FormatUtils.calculateEstimatedArrival(_state.value.currentRoute)
                            }
                        )
                    )
                }
            }

            val currentState = _state.value
            if (currentState.destination != null &&
                currentState.alternativeRoutes.isEmpty() &&
                !currentState.isLoading
            ) {
                setDestination(currentState.destination.latitude, currentState.destination.longitude)
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error updating location", e)
            _state.update {
                it.copy(
                    errorMessage = "Navigation error: ${e.message}",
                    isLocationTracking = false
                )
            }
            locationJob?.cancel()
        }
    }

    fun setDestination(latitude: Double, longitude: Double) {
        viewModelScope.launch {
            try {
                _state.update { it.copy(isLoading = true) }

                _state.update {
                    it.copy(
                        destination = Location(latitude, longitude),
                        errorMessage = null
                    )
                }

                val success = navigationEngine.setDestination(latitude, longitude)
                if (success) {
                    val routes = navigationEngine.getAlternativeRoutes()
                    _state.update {
                        it.copy(
                            alternativeRoutes = routes,
                            isLoading = false
                        )
                    }

                    if (routes.isNotEmpty()) {
                        selectRoute(routes.first().id)
                    } else {
                        Log.w(TAG, "No routes returned despite success flag")
                        _state.update {
                            it.copy(
                                errorMessage = "No routes found to destination"
                            )
                        }
                    }
                } else {
                    Log.e(TAG, "Failed to set destination")
                    _state.update {
                        it.copy(
                            errorMessage = "Could not calculate route to destination",
                            isLoading = false
                        )
                    }
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error setting destination", e)
                _state.update {
                    it.copy(
                        errorMessage = "Error: ${e.message}",
                        isLoading = false
                    )
                }
            }
        }
    }

    fun selectRoute(routeId: String) {
        viewModelScope.launch {
            try {
                val selectedRoute = _state.value.alternativeRoutes.find { it.id == routeId }
                if (selectedRoute == null) {
                    _state.update {
                        it.copy(
                            errorMessage = "Route not found"
                        )
                    }
                    return@launch
                }

                val success = navigationEngine.switchToRoute(routeId)
                if (success) {
                    _state.update {
                        it.copy(
                            currentRoute = selectedRoute,
                            errorMessage = null
                        )
                    }
                } else {
                    _state.update {
                        it.copy(
                            errorMessage = "Failed to select route"
                        )
                    }
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error selecting route", e)
                _state.update {
                    it.copy(
                        errorMessage = "Error: ${e.message}"
                    )
                }
            }
        }
    }

    fun startNavigation() {
        if (_state.value.currentRoute != null) {
            _state.update {
                it.copy(
                    isNavigating = true,
                    errorMessage = null,
                    zoomToRoute = true
                )
            }
            startLocationUpdates()
        } else {
            _state.update {
                it.copy(
                    errorMessage = "No route selected"
                )
            }
        }
    }

    fun stopNavigation() {
        _state.update {
            it.copy(
                isNavigating = false,
                currentRouteMatch = null,
                errorMessage = null,
                zoomToRoute = false
            )
        }
        startLocationUpdates()
    }

    fun clearError() {
        _state.update { it.copy(errorMessage = null) }
    }

    fun clearDestination() {
        viewModelScope.launch {
            try {
                _state.update {
                    it.copy(
                        destination = null,
                        currentRoute = null,
                        alternativeRoutes = emptyList(),
                        currentRouteMatch = null,
                        isNavigating = false,
                        errorMessage = null,
                        zoomToRoute = false,
                        isLoading = false
                    )
                }

                if (_isDemoMode.value) {
                    stopSimulation()
                }

                locationJob?.cancel()
                startLocationUpdates()

                Log.d(TAG, "Navigation state reset")
            } catch (e: Exception) {
                Log.e(TAG, "Error clearing destination", e)
                _state.update {
                    it.copy(
                        errorMessage = "Error resetting: ${e.message}"
                    )
                }
            }
        }
    }

    override fun onCleared() {
        super.onCleared()
        locationJob?.cancel()
        navigationSimulator.stopSimulation()
    }

    private val _isMapDataLoading = MutableStateFlow(true)
    val isMapDataLoading: StateFlow<Boolean> = _isMapDataLoading.asStateFlow()

    private val _mapLoadingProgress = MutableStateFlow(0)
    val mapLoadingProgress: StateFlow<Int> = _mapLoadingProgress.asStateFlow()

    fun updateMapLoadingProgress(progress: Int) {
        _mapLoadingProgress.value = progress
        if (progress >= 100) {
            _isMapDataLoading.value = false
        }
    }

    fun setMapDataLoading(isLoading: Boolean) {
        _isMapDataLoading.value = isLoading
        if (!isLoading) {
            _mapLoadingProgress.value = 100
        }
    }

    fun onMapDataLoaded() {
        _isMapDataLoading.value = false
        _mapLoadingProgress.value = 100
    }

    fun setError(message: String) {
        _state.update { it.copy(errorMessage = message) }
    }
}
