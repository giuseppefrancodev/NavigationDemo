package com.example.navigation.domain.models

data class Location(
    val latitude: Double,
    val longitude: Double,
    val bearing: Float = 0f,
    val speed: Float = 0f,
    val accuracy: Float = 0f
)