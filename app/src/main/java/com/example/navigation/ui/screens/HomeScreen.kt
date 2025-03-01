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
import androidx.compose.ui.unit.dp
import androidx.hilt.navigation.compose.hiltViewModel
import com.example.navigation.ui.components.MapView
import com.example.navigation.ui.components.NavigationPanel
import com.example.navigation.ui.components.RoutePreview
import com.example.navigation.ui.components.DebugPanel
import com.example.navigation.ui.components.LocationPermissionRequest
import com.example.navigation.ui.components.checkAndRequestLocationPermissions
import com.example.navigation.viewmodels.NavigationViewModel
import com.google.android.gms.maps.model.CameraPosition
import com.google.android.gms.maps.model.LatLng
import com.google.maps.android.compose.rememberCameraPositionState
import android.Manifest
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import kotlinx.coroutines.launch

@Composable
fun HomeScreen(
    viewModel: NavigationViewModel = hiltViewModel()
) {
    val state by viewModel.state.collectAsState()
    val isDemoMode by viewModel.isDemoMode.collectAsState()
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val snackbarHostState = remember { SnackbarHostState() }

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

    // Update camera position when location changes - but only if not in demo mode
    // (demo mode has its own camera control in MapView)
    LaunchedEffect(state.currentLocation) {
        if (!isDemoMode) {
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
            },
            isDemoMode = isDemoMode,  // Pass demo mode state to MapView
            followCurrentLocation = isDemoMode // Follow current location in demo mode
        )

        // Loading indicator
        if (state.isLoading) {
            CircularProgressIndicator(
                modifier = Modifier
                    .size(50.dp)
                    .align(Alignment.Center)
            )
        }

        // Show route preview only if not navigating and not in simulation mode
        if (!state.isNavigating && state.currentRoute != null && !isDemoMode) {
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

        // Active navigation panel - show for both regular navigation and simulation
        AnimatedVisibility(
            visible = state.isNavigating || isDemoMode,
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

        // Debug Panel with original function signature
        DebugPanel(
            state = state,
            viewModel = viewModel,
            onClose = {}, // We don't need this anymore but keeping for compatibility
            modifier = Modifier
                .align(Alignment.TopStart)
                .padding(16.dp)
        )

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