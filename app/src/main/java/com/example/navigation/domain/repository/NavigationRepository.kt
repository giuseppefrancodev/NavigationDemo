package com.example.navigation.domain.repository

import android.location.Location as AndroidLocation
import android.os.Looper
import android.util.Log
import com.example.navigation.domain.models.Location
import com.google.android.gms.location.FusedLocationProviderClient
import com.google.android.gms.location.LocationAvailability
import com.google.android.gms.location.LocationCallback
import com.google.android.gms.location.LocationRequest
import com.google.android.gms.location.LocationResult
import com.google.android.gms.location.Priority
import kotlinx.coroutines.channels.awaitClose
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.callbackFlow
import javax.inject.Inject
import javax.inject.Singleton

private const val TAG = "NavigationRepository"

@Singleton
class NavigationRepository @Inject constructor(
    private val fusedLocationClient: FusedLocationProviderClient
) {
    /**
     * Provides location updates
     */
    fun getLocationUpdates(): Flow<Location> = callbackFlow {
        // Create location request
        val locationRequest = LocationRequest.Builder(Priority.PRIORITY_HIGH_ACCURACY, 1000)
            .setMinUpdateIntervalMillis(500)
            .setWaitForAccurateLocation(false)
            .build()

        Log.d(TAG, "Creating location request")

        val locationCallback = object : LocationCallback() {
            override fun onLocationResult(result: LocationResult) {
                result.lastLocation?.let {
                    Log.d(TAG, "Location update: ${it.latitude}, ${it.longitude}, " +
                            "acc: ${it.accuracy}")

                    trySend(it.toLocation())
                } ?: Log.w(TAG, "Received null location")
            }

            override fun onLocationAvailability(locationAvailability: LocationAvailability) {
                val available = locationAvailability.isLocationAvailable
                Log.d(TAG, "Location availability: $available")
            }
        }

        try {
            Log.d(TAG, "Requesting location updates")
            fusedLocationClient.requestLocationUpdates(
                locationRequest,
                locationCallback,
                Looper.getMainLooper()
            )

            // Get last known location to speed things up
            fusedLocationClient.lastLocation.addOnSuccessListener { location ->
                location?.let {
                    Log.d(TAG, "Last known location: ${it.latitude}, ${it.longitude}")
                    trySend(it.toLocation())
                }
            }
        } catch (e: SecurityException) {
            Log.e(TAG, "Security exception requesting location updates", e)
            close(e)
        } catch (e: Exception) {
            Log.e(TAG, "Error requesting location updates", e)
            close(e)
        }

        awaitClose {
            Log.d(TAG, "Removing location updates")
            fusedLocationClient.removeLocationUpdates(locationCallback)
        }
    }

    private fun AndroidLocation.toLocation(): Location {
        return Location(
            latitude = latitude,
            longitude = longitude,
            bearing = bearing,
            speed = speed,
            accuracy = accuracy
        )
    }
}