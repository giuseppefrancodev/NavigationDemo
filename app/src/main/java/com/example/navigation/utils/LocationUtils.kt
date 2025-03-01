package com.example.navigation.utils

import com.example.navigation.domain.models.Location
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.sin
import kotlin.math.sqrt

/**
 * Utility methods for location calculations.
 * This centralizes all the location-related math that was previously
 * duplicated across multiple files.
 */
object LocationUtils {
    // Earth radius in meters
    private const val EARTH_RADIUS_METERS = 6371000.0
    // Earth radius in kilometers
    private const val EARTH_RADIUS_KM = 6371.0

    /**
     * Calculates distance between two locations in meters using the Haversine formula.
     */
    fun calculateDistanceInMeters(
        lat1: Double, lon1: Double,
        lat2: Double, lon2: Double
    ): Double {
        val φ1 = Math.toRadians(lat1)
        val φ2 = Math.toRadians(lat2)
        val Δφ = Math.toRadians(lat2 - lat1)
        val Δλ = Math.toRadians(lon2 - lon1)

        val a = sin(Δφ / 2) * sin(Δφ / 2) +
                cos(φ1) * cos(φ2) *
                sin(Δλ / 2) * sin(Δλ / 2)
        val c = 2 * atan2(sqrt(a), sqrt(1 - a))

        return EARTH_RADIUS_METERS * c
    }

    /**
     * Calculates distance between two locations in meters.
     */
    fun calculateDistanceInMeters(start: Location, end: Location): Double {
        return calculateDistanceInMeters(
            start.latitude, start.longitude,
            end.latitude, end.longitude
        )
    }

    /**
     * Calculates distance between two locations in kilometers.
     */
    fun calculateDistanceInKilometers(start: Location, end: Location): Double {
        val lat1 = Math.toRadians(start.latitude)
        val lng1 = Math.toRadians(start.longitude)
        val lat2 = Math.toRadians(end.latitude)
        val lng2 = Math.toRadians(end.longitude)

        val dLat = lat2 - lat1
        val dLng = lng2 - lng1

        val a = sin(dLat / 2) * sin(dLat / 2) +
                cos(lat1) * cos(lat2) * sin(dLng / 2) * sin(dLng / 2)
        val c = 2 * atan2(sqrt(a), sqrt(1 - a))

        return EARTH_RADIUS_KM * c
    }

    /**
     * Calculates the bearing between two points in degrees (0-360).
     */
    fun calculateBearing(lat1: Double, lon1: Double, lat2: Double, lon2: Double): Double {
        val lat1Rad = Math.toRadians(lat1)
        val lat2Rad = Math.toRadians(lat2)
        val lonDiff = Math.toRadians(lon2 - lon1)

        val y = sin(lonDiff) * cos(lat2Rad)
        val x = cos(lat1Rad) * sin(lat2Rad) - sin(lat1Rad) * cos(lat2Rad) * cos(lonDiff)

        var bearing = Math.toDegrees(atan2(y, x))

        // Normalize to 0-360
        bearing = (bearing + 360) % 360

        return bearing
    }

    /**
     * Calculates the bearing between two locations in degrees (0-360).
     */
    fun calculateBearing(start: Location, end: Location): Double {
        return calculateBearing(
            start.latitude, start.longitude,
            end.latitude, end.longitude
        )
    }

    /**
     * Formats a distance in meters to a human-readable string.
     * For distances ≥ 1000 meters, the result is shown in kilometers with 1 decimal place.
     * For distances < 1000 meters, the result is shown in whole meters.
     */
    fun formatDistance(distanceMeters: Int): String {
        return when {
            distanceMeters >= 1000 -> String.format("%.1f km", distanceMeters / 1000.0)
            else -> "$distanceMeters m"
        }
    }

    /**
     * Formats a distance in meters to a human-readable string.
     * For distances ≥ 1000 meters, the result is shown in kilometers with 1 decimal place.
     * For distances < 1000 meters, the result is shown in whole meters.
     */
    fun formatDistance(distanceMeters: Double): String {
        return when {
            distanceMeters >= 1000 -> String.format("%.1f km", distanceMeters / 1000.0)
            else -> "${distanceMeters.toInt()} m"
        }
    }
}