package com.example.navigation.domain.repository

import com.example.navigation.domain.models.Location
import com.example.navigation.interfaces.NavigationRepositoryInterface
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.MutableSharedFlow

/**
 * A fake implementation of [NavigationRepositoryInterface] for testing.
 * It allows you to emit location updates on-demand.
 */
class FakeNavigationRepository : NavigationRepositoryInterface {

    private val _locationFlow = MutableSharedFlow<Location>(replay = 1)

    override fun getLocationUpdates(): Flow<Location> = _locationFlow

    /**
     * Test helper to emit locations into the flow.
     */
    suspend fun emitLocation(location: Location) {
        _locationFlow.emit(location)
    }
}
