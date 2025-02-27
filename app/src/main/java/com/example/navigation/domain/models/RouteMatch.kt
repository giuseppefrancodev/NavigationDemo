package com.example.navigation.domain.models

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
        get() = when {
            distanceToNext >= 1000 -> String.format("%.1f km", distanceToNext / 1000.0)
            else -> "$distanceToNext m"
        }

    val location: Location
        get() = Location(
            latitude = matchedLatitude,
            longitude = matchedLongitude,
            bearing = matchedBearing
        )
}
