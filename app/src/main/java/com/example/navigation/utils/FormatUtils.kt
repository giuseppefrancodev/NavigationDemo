package com.example.navigation.utils

import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import com.example.navigation.domain.models.Route

/**
 * Utility class for formatting values for display
 */
object FormatUtils {
    /**
     * Formats a duration in seconds as a human-readable string
     * Examples: "5 min", "1 h 30 min"
     */
    fun formatDuration(durationSeconds: Int): String {
        val hours = durationSeconds / 3600
        val minutes = (durationSeconds % 3600) / 60
        return when {
            hours > 0 -> "$hours h $minutes min"
            else -> "$minutes min"
        }
    }

    /**
     * Formats a number with a specified number of decimal places
     */
    fun formatDouble(value: Double, decimals: Int): String {
        return "%.${decimals}f".format(Locale.US, value)
    }

    /**
     * Calculates and formats the estimated arrival time based on a route duration
     */
    fun calculateEstimatedArrival(route: Route?): String {
        if (route == null) return ""

        val formatter = SimpleDateFormat("h:mm a", Locale.getDefault())
        val arrivalTime = Date(System.currentTimeMillis() + route.durationSeconds * 1000L)
        return formatter.format(arrivalTime)
    }
}