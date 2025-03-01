package com.example.navigation.ui.components

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.os.Handler
import android.os.Looper
import android.util.Log
import androidx.compose.foundation.layout.Box
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.core.content.ContextCompat
import com.example.navigation.R
import com.example.navigation.domain.models.Location
import com.example.navigation.domain.models.Route
import com.google.android.gms.maps.CameraUpdateFactory
import com.google.android.gms.maps.MapsInitializer
import com.google.android.gms.maps.model.BitmapDescriptor
import com.google.android.gms.maps.model.BitmapDescriptorFactory
import com.google.android.gms.maps.model.CameraPosition
import com.google.android.gms.maps.model.JointType
import com.google.android.gms.maps.model.LatLng
import com.google.android.gms.maps.model.LatLngBounds
import com.google.maps.android.compose.*
import kotlinx.coroutines.launch

@Composable
fun MapView(
    modifier: Modifier = Modifier,
    cameraPositionState: CameraPositionState,
    currentLocation: Location?,
    destination: Location?,
    currentRoute: Route?,
    alternativeRoutes: List<Route>,
    onMapLongClick: (LatLng) -> Unit,
    isDemoMode: Boolean = false,
    followCurrentLocation: Boolean = false
) {
    val coroutineScope = rememberCoroutineScope()
    val context = LocalContext.current

    // State for map loading
    var isMapLoaded by remember { mutableStateOf(false) }

    // State for map initialization
    var isMapInitialized by remember { mutableStateOf(false) }

    // State for car icon
    var carIconBitmap by remember { mutableStateOf<BitmapDescriptor?>(null) }

    // Keep track of whether we've shown the initial route
    val initialRouteShown = remember { mutableStateOf(false) }

    // Initialize Maps before rendering
    LaunchedEffect(Unit) {
        try {
            // Initialize Maps engine
            MapsInitializer.initialize(context)
            // Wait a moment for complete initialization
            Handler(Looper.getMainLooper()).postDelayed({
                isMapInitialized = true
            }, 500)
        } catch (e: Exception) {
            Log.e("MapView", "Error initializing Maps", e)
        }
    }

    // Create car icon once map is initialized
    LaunchedEffect(isMapInitialized) {
        if (isMapInitialized) {
            try {
                // Convert drawable to bitmap descriptor
                carIconBitmap = getBitmapDescriptorFromVector(context, R.drawable.ic_car_marker)
                Log.d("MapView", "Car icon created successfully")
            } catch (e: Exception) {
                Log.e("MapView", "Error creating car icon", e)
            }
        }
    }

    // --- One-time effect to show the entire route initially ---
    LaunchedEffect(Unit) {
        if (isDemoMode && currentLocation != null && destination != null && !initialRouteShown.value) {
            coroutineScope.launch {
                try {
                    // Build bounds to show from currentLocation to destination (and any route points)
                    val boundsBuilder = LatLngBounds.builder()
                        .include(LatLng(currentLocation.latitude, currentLocation.longitude))
                        .include(LatLng(destination.latitude, destination.longitude))

                    if (currentRoute?.points?.isNotEmpty() == true) {
                        currentRoute.points.forEach { point ->
                            boundsBuilder.include(LatLng(point.latitude, point.longitude))
                        }
                    }

                    val bounds = boundsBuilder.build()

                    // Animate to show the entire route
                    cameraPositionState.animate(
                        update = CameraUpdateFactory.newLatLngBounds(bounds, 100),
                        durationMs = 1000
                    )
                } catch (e: Exception) {
                    // If building bounds fails, fallback to current location
                    val fallbackPosition = CameraPosition.Builder()
                        .target(LatLng(currentLocation.latitude, currentLocation.longitude))
                        .zoom(15f)
                        .build()
                    cameraPositionState.animate(
                        update = CameraUpdateFactory.newCameraPosition(fallbackPosition),
                        durationMs = 500
                    )
                } finally {
                    // Mark the initial route as shown
                    initialRouteShown.value = true
                }
            }
        } else {
            // If not demo mode or missing data, we can consider it 'shown' by default
            initialRouteShown.value = true
        }
    }

    // --- Follow current location once the initial route is shown ---
    LaunchedEffect(currentLocation, isDemoMode, followCurrentLocation, initialRouteShown.value) {
        // If we are in demo mode, have a current location, want to follow it,
        // and the initial route has been shown, then animate to current location.
        if (isDemoMode && followCurrentLocation && currentLocation != null && initialRouteShown.value) {
            coroutineScope.launch {
                val position = CameraPosition.Builder()
                    .target(LatLng(currentLocation.latitude, currentLocation.longitude))
                    .zoom(17f)
                    .bearing(currentLocation.bearing)
                    .tilt(45f)
                    .build()

                cameraPositionState.animate(
                    update = CameraUpdateFactory.newCameraPosition(position),
                    durationMs = 500
                )
            }
        }
    }

    // Show map with loading indicator until initialized
    Box(modifier = modifier) {
        GoogleMap(
            modifier = Modifier.matchParentSize(),
            cameraPositionState = cameraPositionState,
            onMapLongClick = if (!isDemoMode) onMapLongClick else { _ -> },
            uiSettings = MapUiSettings(
                zoomControlsEnabled = !isDemoMode,
                scrollGesturesEnabled = !isDemoMode,
                zoomGesturesEnabled = !isDemoMode
            ),
            onMapLoaded = {
                isMapLoaded = true
            }
        ) {
            // Only draw markers and routes when map is loaded
            if (isMapLoaded) {
                // Draw current location marker with car icon
                currentLocation?.let { location ->
                    val position = LatLng(location.latitude, location.longitude)

                    Marker(
                        state = MarkerState(position),
                        title = "Current Location",
                        // Use car icon if available, or fall back to blue marker
                        icon = carIconBitmap ?: BitmapDescriptorFactory.defaultMarker(BitmapDescriptorFactory.HUE_AZURE),
                        rotation = location.bearing,
                        anchor = androidx.compose.ui.geometry.Offset(0.5f, 0.5f),
                        flat = true,
                        zIndex = 1f
                    )
                }

                // Draw destination marker
                destination?.let { location ->
                    val position = LatLng(location.latitude, location.longitude)
                    Marker(
                        state = MarkerState(position),
                        title = "Destination"
                    )
                }

                // Draw route path
                currentRoute?.let { route ->
                    if (route.points.isNotEmpty()) {
                        val routePoints = route.points.map { LatLng(it.latitude, it.longitude) }
                        Polyline(
                            points = routePoints,
                            color = Color.Blue,
                            width = 5f,
                            jointType = JointType.ROUND,
                            startCap = com.google.android.gms.maps.model.RoundCap(),
                            endCap = com.google.android.gms.maps.model.RoundCap()
                        )
                    } else if (currentLocation != null && destination != null) {
                        Polyline(
                            points = listOf(
                                LatLng(currentLocation.latitude, currentLocation.longitude),
                                LatLng(destination.latitude, destination.longitude)
                            ),
                            color = Color.Blue,
                            width = 5f,
                            jointType = JointType.ROUND
                        )
                    }
                }
            }
        }

        // Show loading indicator while map is initializing
        if (!isMapLoaded) {
            CircularProgressIndicator(
                modifier = Modifier.align(Alignment.Center)
            )
        }
    }
}

// Helper function to convert vector drawable to bitmap descriptor
private fun getBitmapDescriptorFromVector(context: Context, drawableId: Int): BitmapDescriptor? {
    return try {
        val drawable = ContextCompat.getDrawable(context, drawableId) ?: return null

        val bitmap = Bitmap.createBitmap(
            drawable.intrinsicWidth,
            drawable.intrinsicHeight,
            Bitmap.Config.ARGB_8888
        )

        val canvas = Canvas(bitmap)
        drawable.setBounds(0, 0, canvas.width, canvas.height)
        drawable.draw(canvas)

        BitmapDescriptorFactory.fromBitmap(bitmap)
    } catch (e: Exception) {
        Log.e("MapView", "Error creating bitmap from vector", e)
        null
    }
}