import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.slideOutVertically
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.hilt.navigation.compose.hiltViewModel
import com.example.navigation.ui.components.MapView
import com.example.navigation.ui.components.NavigationPanel
import com.example.navigation.ui.components.RoutePreview
import com.example.navigation.viewmodels.NavigationViewModel
import com.google.android.gms.maps.model.CameraPosition
import com.google.android.gms.maps.model.LatLng
import com.google.maps.android.compose.rememberCameraPositionState
import android.Manifest
import android.content.pm.PackageManager
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Info
import androidx.compose.material.icons.filled.LocationOn
import androidx.compose.material3.Card
import androidx.compose.material3.FloatingActionButton
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Snackbar
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.ui.graphics.Color
import androidx.core.content.ContextCompat
import kotlinx.coroutines.launch

@Composable
fun HomeScreen(
    viewModel: NavigationViewModel = hiltViewModel()
) {
    val state by viewModel.state.collectAsState()
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val snackbarHostState = remember { SnackbarHostState() }

    // Debug mode toggle
    var showDebugPanel by remember { mutableStateOf(false) }

    // Error handling
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

    // Location permission handling
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

    // Camera position management
    val defaultLocation = LatLng(37.7749, -122.4194) // San Francisco
    val currentLocation = state.currentLocation?.let {
        LatLng(it.latitude, it.longitude)
    } ?: defaultLocation

    val cameraPositionState = rememberCameraPositionState {
        position = CameraPosition.fromLatLngZoom(currentLocation, 15f)
    }

    // Update camera position when location changes if navigating or no camera movement by user
    LaunchedEffect(state.currentLocation) {
        state.currentLocation?.let {
            // Only update camera if we're navigating or camera hasn't moved recently
            if (state.isNavigating || !cameraPositionState.isMoving) {
                cameraPositionState.position = CameraPosition.fromLatLngZoom(
                    LatLng(it.latitude, it.longitude),
                    15f
                )
            }
        }
    }

    Box(modifier = Modifier.fillMaxSize()) {
        // Main map view
        MapView(
            modifier = Modifier.fillMaxSize(),
            cameraPositionState = cameraPositionState,
            currentLocation = state.currentLocation,
            destination = state.destination,
            currentRoute = state.currentRoute,
            alternativeRoutes = state.alternativeRoutes,
            onMapLongClick = { latLng ->
                viewModel.setDestination(latLng.latitude, latLng.longitude)
            }
        )

        // Loading indicator
        if (state.isLoading) {
            CircularProgressIndicator(
                modifier = Modifier
                    .size(50.dp)
                    .align(Alignment.Center)
            )
        }

        // Debug info panel - only shown when toggled
        AnimatedVisibility(
            visible = showDebugPanel,
            enter = fadeIn() + slideInVertically { -it },
            exit = fadeOut() + slideOutVertically { -it },
            modifier = Modifier.align(Alignment.TopEnd)
        ) {
            DebugPanel(
                state = state,
                viewModel = viewModel,
                onClose = { showDebugPanel = false }
            )
        }

        // Navigation controls
        if (!state.isNavigating && state.currentRoute != null) {
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

        // Active navigation panel
        AnimatedVisibility(
            visible = state.isNavigating,
            enter = fadeIn() + slideInVertically { it },
            exit = fadeOut() + slideOutVertically { it },
            modifier = Modifier
                .align(Alignment.BottomCenter)
                .padding(16.dp)
        ) {
            NavigationPanel(
                routeMatch = state.currentRouteMatch,
                onStopNavigation = { viewModel.stopNavigation() }
            )
        }

        // Permission message if needed
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

        // Floating action buttons moved to top left
        Column(
            modifier = Modifier
                .align(Alignment.TopStart) // Align to top left
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            // My Location button
            FloatingActionButton(
                onClick = {
                    state.currentLocation?.let {
                        cameraPositionState.position = CameraPosition.fromLatLngZoom(
                            LatLng(it.latitude, it.longitude),
                            15f
                        )
                    }
                }
            ) {
                Icon(
                    imageVector = Icons.Default.LocationOn,
                    contentDescription = "My Location"
                )
            }

            // Debug toggle button
            FloatingActionButton(
                onClick = { showDebugPanel = !showDebugPanel },
                containerColor = if (showDebugPanel) MaterialTheme.colorScheme.primary
                else MaterialTheme.colorScheme.surfaceVariant
            ) {
                Icon(
                    imageVector = Icons.Default.Info,
                    contentDescription = "Toggle Debug Panel"
                )
            }
        }

        // Snackbar host for errors
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

@Composable
private fun DebugPanel(
    state: com.example.navigation.viewmodels.NavigationState,
    viewModel: NavigationViewModel,
    onClose: () -> Unit
) {
    Card(
        modifier = Modifier
            .padding(16.dp)
            .width(220.dp),
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.surface.copy(alpha = 0.9f)
        )
    ) {
        Column(
            modifier = Modifier.padding(12.dp),
            horizontalAlignment = Alignment.Start
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text("DEBUG INFO", fontWeight = FontWeight.Bold)
                IconButton(onClick = onClose, modifier = Modifier.size(24.dp)) {
                    Icon(
                        imageVector = Icons.Default.Close,
                        contentDescription = "Close Debug Panel"
                    )
                }
            }

            Divider(Modifier.padding(vertical = 4.dp))

            Text("Location: ${state.currentLocation?.latitude?.format(4) ?: "None"}, ${state.currentLocation?.longitude?.format(4) ?: "None"}")
            Text("Has Destination: ${state.destination != null}")
            Text("Routes: ${state.alternativeRoutes.size}")
            Text("Current Route: ${state.currentRoute?.id ?: "None"}")
            Text("Is Navigating: ${state.isNavigating}")
            Text("Location Tracking: ${state.isLocationTracking}")

            Spacer(modifier = Modifier.height(8.dp))

            if (state.currentLocation != null && state.destination == null) {
                Button(
                    onClick = {
                        // Set a destination near the current location
                        val currLoc = state.currentLocation!!
                        viewModel.setDestination(
                            currLoc.latitude + 0.005,
                            currLoc.longitude + 0.005
                        )
                    },
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Text("Set Test Destination")
                }
            }

            if (!state.isLocationTracking) {
                Button(
                    onClick = { viewModel.onLocationPermissionGranted(true) }, // Pass `true` to retry
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Text("Retry Location")
                }
            }
        }
    }
}

@Composable
private fun LocationPermissionRequest(onRequestPermission: () -> Unit) {
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color.Black.copy(alpha = 0.5f)),
        contentAlignment = Alignment.Center
    ) {
        Card(
            modifier = Modifier
                .padding(16.dp)
                .width(300.dp)
        ) {
            Column(
                modifier = Modifier.padding(16.dp),
                horizontalAlignment = Alignment.CenterHorizontally
            ) {
                Text(
                    "Location Permission Required",
                    style = MaterialTheme.typography.titleLarge,
                    fontWeight = FontWeight.Bold
                )

                Spacer(modifier = Modifier.height(8.dp))

                Text(
                    "This navigation app needs access to your location to function properly.",
                    style = MaterialTheme.typography.bodyMedium
                )

                Spacer(modifier = Modifier.height(16.dp))

                Button(
                    onClick = onRequestPermission,
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Text("Grant Permission")
                }
            }
        }
    }
}

// Helper functions
private fun checkAndRequestLocationPermissions(
    context: android.content.Context,
    launcher: androidx.activity.result.ActivityResultLauncher<Array<String>>,
    onPermissionGranted: () -> Unit
) {
    val fineLocationPermission = ContextCompat.checkSelfPermission(
        context,
        Manifest.permission.ACCESS_FINE_LOCATION
    )
    val coarseLocationPermission = ContextCompat.checkSelfPermission(
        context,
        Manifest.permission.ACCESS_COARSE_LOCATION
    )

    if (fineLocationPermission == PackageManager.PERMISSION_GRANTED ||
        coarseLocationPermission == PackageManager.PERMISSION_GRANTED) {
        onPermissionGranted()
    } else {
        launcher.launch(
            arrayOf(
                Manifest.permission.ACCESS_FINE_LOCATION,
                Manifest.permission.ACCESS_COARSE_LOCATION
            )
        )
    }
}

// Helper extension function
fun Double.format(digits: Int) = "%.${digits}f".format(this)