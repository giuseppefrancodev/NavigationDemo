/*
 * File: Location.kt
 * Description: Data class representing a geographical location with latitude, longitude, bearing, speed, and accuracy.
 * Author: Giuseppe Franco
 * Created: March 2025
 */

package com.example.navigation.domain.models

data class Location(
    val latitude: Double,
    val longitude: Double,
    val bearing: Float = 0f,
    val speed: Float = 0f,
    val accuracy: Float = 0f
)
