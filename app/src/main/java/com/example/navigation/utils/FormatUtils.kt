/*
 * File: FormatUtils.kt
 * Description: Utility class for formatting navigation-related data such as duration and estimated arrival time.
 * Author: Giuseppe Franco
 * Created: March 2025
 */

package com.example.navigation.utils

import com.example.navigation.domain.models.Route
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

object FormatUtils {

    fun formatDuration(durationSeconds: Int): String {
        val hours = durationSeconds / 3600
        val minutes = (durationSeconds % 3600) / 60
        return when {
            hours > 0 -> "$hours h $minutes min"
            else -> "$minutes min"
        }
    }

    fun calculateEstimatedArrival(route: Route?): String {
        if (route == null) return ""

        val formatter = SimpleDateFormat("h:mm a", Locale.getDefault())
        val arrivalTime = Date(System.currentTimeMillis() + route.durationSeconds * 1000L)
        return formatter.format(arrivalTime)
    }
}
