/*
 * File: Route.kt
 * Description: Data class representing a navigation route with points, duration, and distance calculations.
 * Author: Giuseppe Franco
 * Created: March 2025
 */

package com.example.navigation.domain.models

import com.example.navigation.utils.FormatUtils
import com.example.navigation.utils.LocationUtils

data class Route(
    val id: String,
    val points: List<Location>,
    val durationSeconds: Int,
    val name: String
) {
    val durationFormatted: String
        get() = FormatUtils.formatDuration(durationSeconds)

    private val distanceMeters: Int
        get() {
            var distance = 0.0
            for (i in 0 until points.size - 1) {
                distance += LocationUtils.calculateDistanceInMeters(
                    points[i].latitude, points[i].longitude,
                    points[i + 1].latitude, points[i + 1].longitude
                )
            }
            return distance.toInt()
        }

    val distanceFormatted: String
        get() = LocationUtils.formatDistance(distanceMeters)
}
