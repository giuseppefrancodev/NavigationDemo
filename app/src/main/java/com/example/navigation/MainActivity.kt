/*
 * File: MainActivity.kt
 * Description: Main activity for the navigation app, responsible for loading map data and setting up the UI.
 * Author: Giuseppe Franco
 * Created: March 2025
 */

package com.example.navigation

import HomeScreen
import android.os.Bundle
import android.util.Log
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.viewModels
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.ui.Modifier
import androidx.lifecycle.lifecycleScope
import com.example.navigation.interfaces.NavigationEngineInterface
import com.example.navigation.ui.theme.NavigationDemoTheme
import com.example.navigation.viewmodels.NavigationViewModel
import dagger.hilt.android.AndroidEntryPoint
import javax.inject.Inject
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

private const val TAG = "MainActivity"

@AndroidEntryPoint
class MainActivity : ComponentActivity() {
    @Inject
    lateinit var navigationEngine: NavigationEngineInterface
    private val viewModel: NavigationViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContent {
            NavigationDemoTheme {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    HomeScreen(viewModel)
                }
            }
        }

        loadMapData()
    }

    private fun loadMapData() {
        // First set the loading state
        viewModel.setMapDataLoading(true)

        lifecycleScope.launch {
            try {
                withContext(Dispatchers.IO) {
                    try {
                        val success = navigationEngine.loadOSMDataFromAssets("lauttasaari_roads.osm")
                        Log.d(TAG, "OSM data loaded: $success")
                        withContext(Dispatchers.Main) {
                            viewModel.onMapDataLoaded()
                            if (!success) {
                                viewModel.setError(
                                    "Failed to load map data. Please check if lauttasaari.osm is in the assets folder."
                                )
                            }
                        }
                    } catch (e: Exception) {
                        Log.e(TAG, "Error loading map data", e)
                        withContext(Dispatchers.Main) {
                            viewModel.setError("Error loading map data: ${e.message}")
                            viewModel.setMapDataLoading(false)
                        }
                    }
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error in loadMapData", e)
                viewModel.setMapDataLoading(false)
                viewModel.setError("Error loading map data: ${e.message}")
            }
        }
    }
}
