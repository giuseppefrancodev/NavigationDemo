package com.example.navigation.domain.models

data class Route(
    val id: String,
    val points: List<Location>,
    val durationSeconds: Int,
    val name: String
) {
    val durationFormatted: String
        get() {
            val hours = durationSeconds / 3600
            val minutes = (durationSeconds % 3600) / 60
            return when {
                hours > 0 -> "$hours h $minutes min"
                else -> "$minutes min"
            }
        }

    val distanceMeters: Int
        get() {
            var distance = 0.0
            for (i in 0 until points.size - 1) {
                distance += calculateDistance(
                    points[i].latitude, points[i].longitude,
                    points[i + 1].latitude, points[i + 1].longitude
                )
            }
            return distance.toInt()
        }

    val distanceFormatted: String
        get() {
            val distance = distanceMeters
            return when {
                distance >= 1000 -> String.format("%.1f km", distance / 1000.0)
                else -> "$distance m"
            }
        }

    private fun calculateDistance(lat1: Double, lon1: Double, lat2: Double, lon2: Double): Double {
        val r = 6371e3 // Earth radius in meters
        val φ1 = lat1 * Math.PI / 180
        val φ2 = lat2 * Math.PI / 180
        val Δφ = (lat2 - lat1) * Math.PI / 180
        val Δλ = (lon2 - lon1) * Math.PI / 180

        val a = Math.sin(Δφ / 2) * Math.sin(Δφ / 2) +
                Math.cos(φ1) * Math.cos(φ2) *
                Math.sin(Δλ / 2) * Math.sin(Δλ / 2)
        val c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a))

        return r * c
    }
}