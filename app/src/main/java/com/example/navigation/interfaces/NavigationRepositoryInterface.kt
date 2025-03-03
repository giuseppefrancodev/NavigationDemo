package com.example.navigation.interfaces

import com.example.navigation.domain.models.Location
import kotlinx.coroutines.flow.Flow

/**
 * NavigationRepositoryInterface provides an abstraction for location updates.
 * Classes implementing this interface can retrieve location updates as a Flow.
 */
interface NavigationRepositoryInterface {
    fun getLocationUpdates(): Flow<Location>
}
