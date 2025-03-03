/*
 * File: RouteMatch.kt
 * Description: Data class representing a matched route segment with street name, next maneuver, and distance to the next step.
 * Author: Giuseppe Franco
 * Created: March 2025
 */

package com.example.navigation.domain.models

import com.example.navigation.utils.LocationUtils

data class RouteMatch(
    val streetName: String,
    val nextManeuver: String,
    val distanceToNext: Int,
    val estimatedTimeOfArrival: String,
    val matchedLatitude: Double,
    val matchedLongitude: Double,
    val matchedBearing: Float
) {
    val distanceFormatted: String
        get() = LocationUtils.formatDistance(distanceToNext)

    val location: Location
        get() = Location(
            latitude = matchedLatitude,
            longitude = matchedLongitude,
            bearing = matchedBearing
        )
}
