/*
 * File: AppModule.kt
 * Description: Dependency injection module for providing application-wide dependencies.
 * Author: Giuseppe Franco
 * Created: March 2025
 */

package com.example.navigation.di

import android.content.Context
import com.example.navigation.NavigationEngine
import com.example.navigation.domain.repository.NavigationRepository
import com.example.navigation.interfaces.NavigationEngineInterface
import com.example.navigation.interfaces.NavigationRepositoryInterface
import com.google.android.gms.location.FusedLocationProviderClient
import com.google.android.gms.location.LocationServices
import dagger.Module
import dagger.Provides
import dagger.hilt.InstallIn
import dagger.hilt.android.qualifiers.ApplicationContext
import dagger.hilt.components.SingletonComponent
import javax.inject.Singleton

@Module
@InstallIn(SingletonComponent::class)
object AppModule {

    @Provides
    @Singleton
    fun provideFusedLocationProviderClient(@ApplicationContext context: Context): FusedLocationProviderClient {
        return LocationServices.getFusedLocationProviderClient(context)
    }

    @Provides
    @Singleton
    fun provideNavigationEngine(@ApplicationContext context: Context): NavigationEngineInterface {
        return NavigationEngine(context)
    }

    @Provides
    @Singleton
    fun provideNavigationRepositoryInterface(
        fusedLocationProviderClient: FusedLocationProviderClient
    ): NavigationRepositoryInterface {
        return NavigationRepository(fusedLocationProviderClient)
    }
}
