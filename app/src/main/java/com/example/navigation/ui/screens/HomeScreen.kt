/*
 * File: HomeScreen.kt
 * Description: Main screen of the navigation app, integrating map view, navigation panel, and route preview components.
 * Author: Giuseppe Franco
 * Created: March 2025
 */

import android.Manifest
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.slideOutVertically
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.hilt.navigation.compose.hiltViewModel
import com.example.navigation.ui.components.DebugPanel
import com.example.navigation.ui.components.LocationPermissionRequest
import com.example.navigation.ui.components.MapView
import com.example.navigation.ui.components.NavigationPanel
import com.example.navigation.ui.components.RoutePreview
import com.example.navigation.ui.components.checkAndRequestLocationPermissions
import com.example.navigation.viewmodels.NavigationViewModel
import com.google.android.gms.maps.model.CameraPosition
import com.google.android.gms.maps.model.LatLng
import com.google.maps.android.compose.rememberCameraPositionState
import kotlinx.coroutines.launch

@Composable
fun HomeScreen(viewModel: NavigationViewModel = hiltViewModel()) {
    val state by viewModel.state.collectAsState()
    val isDemoMode by viewModel.isDemoMode.collectAsState()
    val isMapDataLoading by viewModel.isMapDataLoading.collectAsState()
    val mapLoadingProgress by viewModel.mapLoadingProgress.collectAsState()

    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val snackbarHostState = remember { SnackbarHostState() }

    LaunchedEffect(state.errorMessage) {
        state.errorMessage?.let { errorMessage ->
            scope.launch {
                snackbarHostState.showSnackbar(
                    message = errorMessage,
                    actionLabel = "Dismiss"
                )
                viewModel.clearError()
            }
        }
    }

    val launcher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.RequestMultiplePermissions()
    ) { permissions ->
        val locationPermissionsGranted = permissions.entries.all { it.value }
        viewModel.onLocationPermissionGranted(locationPermissionsGranted)
    }

    LaunchedEffect(Unit) {
        checkAndRequestLocationPermissions(context, launcher) {
            viewModel.onLocationPermissionGranted(true)
        }
    }

    val defaultLocation = LatLng(60.16, 24.88)
    val currentLocation = state.currentLocation?.let {
        LatLng(it.latitude, it.longitude)
    } ?: defaultLocation

    val cameraPositionState = rememberCameraPositionState {
        position = CameraPosition.fromLatLngZoom(currentLocation, 15f)
    }

    LaunchedEffect(state.currentLocation) {
        if (!isDemoMode) {
            state.currentLocation?.let {
                if (state.isNavigating || !cameraPositionState.isMoving) {
                    cameraPositionState.position = CameraPosition.fromLatLngZoom(
                        LatLng(it.latitude, it.longitude),
                        15f
                    )
                }
            }
        }
    }

    Box(modifier = Modifier.fillMaxSize()) {
        MapView(
            modifier = Modifier.fillMaxSize(),
            cameraPositionState = cameraPositionState,
            currentLocation = state.currentLocation,
            destination = state.destination,
            currentRoute = state.currentRoute,
            alternativeRoutes = state.alternativeRoutes,
            onMapLongClick = { latLng ->
                if (!isMapDataLoading) {
                    viewModel.setDestination(latLng.latitude, latLng.longitude)
                }
            },
            isDemoMode = isDemoMode,
            followCurrentLocation = isDemoMode
        )

        if (isMapDataLoading) {
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .background(Color.Black.copy(alpha = 0.7f)),
                contentAlignment = Alignment.Center
            ) {
                Card(
                    modifier = Modifier
                        .width(300.dp)
                        .padding(16.dp),
                    elevation = CardDefaults.cardElevation(defaultElevation = 8.dp)
                ) {
                    Column(
                        horizontalAlignment = Alignment.CenterHorizontally,
                        verticalArrangement = Arrangement.Center,
                        modifier = Modifier.padding(24.dp)
                    ) {
                        CircularProgressIndicator()
                        Spacer(modifier = Modifier.height(16.dp))
                        Text(
                            text = "Loading map data...",
                            style = MaterialTheme.typography.bodyLarge,
                            textAlign = TextAlign.Center
                        )
                        if (mapLoadingProgress > 0) {
                            Spacer(modifier = Modifier.height(16.dp))
                            LinearProgressIndicator(
                                progress = { mapLoadingProgress / 100f },
                                modifier = Modifier.fillMaxWidth()
                            )
                            Text(
                                text = "$mapLoadingProgress%",
                                style = MaterialTheme.typography.bodyMedium,
                                modifier = Modifier.padding(top = 8.dp)
                            )
                        }
                    }
                }
            }
        }

        if (state.isLoading && !isMapDataLoading) {
            Box(
                modifier = Modifier.fillMaxSize(),
                contentAlignment = Alignment.Center
            ) {
                CircularProgressIndicator(
                    modifier = Modifier.size(50.dp)
                )
            }
        }

        if (!state.isNavigating && state.currentRoute != null && !isDemoMode && !isMapDataLoading) {
            RoutePreview(
                currentRoute = state.currentRoute,
                alternativeRoutes = state.alternativeRoutes,
                onRouteSelected = { viewModel.selectRoute(it) },
                onStartNavigation = { viewModel.startNavigation() },
                modifier = Modifier
                    .align(Alignment.BottomCenter)
                    .padding(16.dp)
            )
        }

        AnimatedVisibility(
            visible = (state.isNavigating || isDemoMode) && !isMapDataLoading,
            enter = fadeIn() + slideInVertically { it },
            exit = fadeOut() + slideOutVertically { it },
            modifier = Modifier
                .align(Alignment.BottomCenter)
                .padding(16.dp)
        ) {
            NavigationPanel(
                routeMatch = state.currentRouteMatch,
                onStopNavigation = {
                    if (isDemoMode) {
                        viewModel.stopSimulation()
                    } else {
                        viewModel.stopNavigation()
                    }
                }
            )
        }

        if (!state.hasLocationPermission) {
            LocationPermissionRequest(
                onRequestPermission = {
                    launcher.launch(
                        arrayOf(
                            Manifest.permission.ACCESS_FINE_LOCATION,
                            Manifest.permission.ACCESS_COARSE_LOCATION
                        )
                    )
                }
            )
        }

        if (!isMapDataLoading) {
            DebugPanel(
                state = state,
                viewModel = viewModel,
                onClose = {},
                modifier = Modifier
                    .align(Alignment.TopStart)
                    .padding(16.dp)
            )
        }

        SnackbarHost(
            hostState = snackbarHostState,
            modifier = Modifier
                .align(Alignment.BottomCenter)
                .padding(bottom = if (state.isNavigating || state.currentRoute != null) 220.dp else 16.dp),
            snackbar = { data ->
                Snackbar(
                    modifier = Modifier.padding(16.dp),
                    action = {
                        TextButton(onClick = { snackbarHostState.currentSnackbarData?.dismiss() }) {
                            Text(data.visuals.actionLabel ?: "Dismiss")
                        }
                    }
                ) {
                    Text(data.visuals.message)
                }
            }
        )
    }
}
