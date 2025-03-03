/*
 * File: LocationUtils.kt
 * Description: Utility class for calculating distances, bearings, and formatting distance values.
 * Author: Giuseppe Franco
 * Created: March 2025
 */

package com.example.navigation.utils

import java.util.Locale
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.sin
import kotlin.math.sqrt

object LocationUtils {
    private const val EARTH_RADIUS_METERS = 6371000.0

    fun calculateDistanceInMeters(lat1: Double, lon1: Double, lat2: Double, lon2: Double): Double {
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

    fun calculateBearing(lat1: Double, lon1: Double, lat2: Double, lon2: Double): Double {
        val lat1Rad = Math.toRadians(lat1)
        val lat2Rad = Math.toRadians(lat2)
        val lonDiff = Math.toRadians(lon2 - lon1)

        val y = sin(lonDiff) * cos(lat2Rad)
        val x = cos(lat1Rad) * sin(lat2Rad) - sin(lat1Rad) * cos(lat2Rad) * cos(lonDiff)

        var bearing = Math.toDegrees(atan2(y, x))
        bearing = (bearing + 360) % 360

        return bearing
    }

    fun formatDistance(distanceMeters: Int): String {
        val locale = Locale.getDefault()
        return when {
            distanceMeters >= 1000 -> String.format(locale, "%.1f km", distanceMeters / 1000.0)
            else -> "$distanceMeters m"
        }
    }
}
