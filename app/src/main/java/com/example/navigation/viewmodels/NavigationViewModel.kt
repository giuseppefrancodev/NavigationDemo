package com.example.navigation.viewmodels

import android.util.Log
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.example.navigation.NavigationEngine
import com.example.navigation.domain.models.Location
import com.example.navigation.domain.models.Route
import com.example.navigation.domain.models.RouteMatch
import com.example.navigation.domain.repository.NavigationRepository
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.catch
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import javax.inject.Inject

private const val TAG = "NavigationViewModel"

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
    val isLoading: Boolean = false
)

@HiltViewModel
class NavigationViewModel @Inject constructor(
    private val navigationRepository: NavigationRepository,
    private val navigationEngine: NavigationEngine
) : ViewModel() {

    private val _state = MutableStateFlow(NavigationState())
    val state: StateFlow<NavigationState> = _state.asStateFlow()

    private var locationJob: Job? = null

    fun onLocationPermissionGranted(hasPermission: Boolean) {
        _state.update { it.copy(
            hasLocationPermission = hasPermission,
            isLocationTracking = hasPermission,
            errorMessage = if (!hasPermission) "Location permission denied" else null
        )}
        if (hasPermission) {
            startLocationUpdates()
        }
    }

    private fun startLocationUpdates() {
        locationJob?.cancel() // Cancel existing job to avoid leaks
        locationJob = viewModelScope.launch {
            try {
                _state.update { it.copy(isLocationTracking = true) }

                // Get location updates
                navigationRepository.getLocationUpdates()
                    .catch { e ->
                        Log.e(TAG, "Error in location updates", e)
                        _state.update { it.copy(
                            errorMessage = "Location error: ${e.message}",
                            isLocationTracking = false
                        )}
                        // Retry location updates after a delay
                        delay(1000) // Wait for 1 second before retrying
                        startLocationUpdates() // Retry
                    }
                    .collect { location ->
                        processNewLocation(location)
                    }
            } catch (e: Exception) {
                Log.e(TAG, "Failed to start location updates", e)
                _state.update { it.copy(
                    errorMessage = "Failed to get location: ${e.message}",
                    isLocationTracking = false
                )}
                // Retry location updates after a delay
                delay(1000) // Wait for 1 second before retrying
                startLocationUpdates() // Retry
            }
        }
    }

    private fun processNewLocation(location: Location) {
        val currentLocation = _state.value.currentLocation
        if (currentLocation != null &&
            currentLocation.latitude == location.latitude &&
            currentLocation.longitude == location.longitude) {
            return // Skip update if location hasn't changed
        }

        try {
            _state.update { it.copy(currentLocation = location) }

            val routeMatch = navigationEngine.updateLocation(
                location.latitude,
                location.longitude,
                location.bearing,
                location.speed,
                location.accuracy
            )

            if (_state.value.isNavigating) {
                _state.update { it.copy(
                    currentRouteMatch = routeMatch.copy(
                        estimatedTimeOfArrival = if (routeMatch.estimatedTimeOfArrival.isBlank())
                            calculateEstimatedArrival(_state.value.currentRoute)
                        else
                            routeMatch.estimatedTimeOfArrival
                    )
                )}
            }

            val currentState = _state.value
            if (currentState.destination != null &&
                currentState.alternativeRoutes.isEmpty() &&
                !currentState.isLoading) {
                setDestination(currentState.destination.latitude, currentState.destination.longitude)
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error updating location", e)
            _state.update { it.copy(
                errorMessage = "Navigation error: ${e.message}",
                isLocationTracking = false // Stop location updates on critical error
            )}
            locationJob?.cancel() // Cancel the job to stop further updates
        }
    }

    fun setDestination(latitude: Double, longitude: Double) {
        viewModelScope.launch {
            try {
                _state.update { it.copy(isLoading = true) }

                _state.update { it.copy(
                    destination = Location(latitude, longitude),
                    errorMessage = null
                )}

                val success = navigationEngine.setDestination(latitude, longitude)
                if (success) {
                    val routes = navigationEngine.getAlternativeRoutes()
                    _state.update { it.copy(
                        alternativeRoutes = routes,
                        isLoading = false
                    )}

                    if (routes.isNotEmpty()) {
                        selectRoute(routes.first().id)
                    } else {
                        Log.w(TAG, "No routes returned despite success flag")
                        _state.update { it.copy(
                            errorMessage = "No routes found to destination"
                        )}
                    }
                } else {
                    Log.e(TAG, "Failed to set destination")
                    _state.update { it.copy(
                        errorMessage = "Could not calculate route to destination",
                        isLoading = false
                    )}
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error setting destination", e)
                _state.update { it.copy(
                    errorMessage = "Error: ${e.message}",
                    isLoading = false
                )}
            }
        }
    }

    fun selectRoute(routeId: String) {
        viewModelScope.launch {
            try {
                val selectedRoute = _state.value.alternativeRoutes.find { it.id == routeId }
                if (selectedRoute == null) {
                    _state.update { it.copy(
                        errorMessage = "Route not found"
                    )}
                    return@launch
                }

                val success = navigationEngine.switchToRoute(routeId)
                if (success) {
                    _state.update { it.copy(
                        currentRoute = selectedRoute,
                        errorMessage = null
                    )}
                } else {
                    _state.update { it.copy(
                        errorMessage = "Failed to select route"
                    )}
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error selecting route", e)
                _state.update { it.copy(
                    errorMessage = "Error: ${e.message}"
                )}
            }
        }
    }

    fun startNavigation() {
        if (_state.value.currentRoute != null) {
            _state.update { it.copy(
                isNavigating = true,
                errorMessage = null
            )}
            startLocationUpdates()
        } else {
            _state.update { it.copy(
                errorMessage = "No route selected"
            )}
        }
    }

    fun stopNavigation() {
        _state.update { it.copy(
            isNavigating = false,
            currentRouteMatch = null,
            errorMessage = null
        )}
        startLocationUpdates()
    }

    fun clearError() {
        _state.update { it.copy(errorMessage = null) }
    }

    private fun calculateEstimatedArrival(route: Route?): String {
        if (route == null) return ""

        val formatter = SimpleDateFormat("h:mm a", Locale.getDefault())
        val arrivalTime = Date(System.currentTimeMillis() + route.durationSeconds * 1000L)
        return formatter.format(arrivalTime)
    }

    override fun onCleared() {
        super.onCleared()
        locationJob?.cancel()
    }
}