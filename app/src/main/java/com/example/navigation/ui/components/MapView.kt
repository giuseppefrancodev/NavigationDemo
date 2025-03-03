/*
 * File: MapView.kt
 * Description: Composable component for displaying a map with navigation routes, markers, and location tracking.
 * Author: Giuseppe Franco
 * Created: March 2025
 */

package com.example.navigation.ui.components

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.os.Handler
import android.os.Looper
import android.util.Log
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
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

    var isMapLoaded by remember { mutableStateOf(false) }

    var isMapInitialized by remember { mutableStateOf(false) }

    var carIconBitmap by remember { mutableStateOf<BitmapDescriptor?>(null) }

    var fixedCarNavigationMode by remember { mutableStateOf(isDemoMode && followCurrentLocation) }

    val initialRouteShown = remember { mutableStateOf(false) }

    LaunchedEffect(Unit) {
        try {
            MapsInitializer.initialize(context)

            Handler(Looper.getMainLooper()).postDelayed({
                isMapInitialized = true
            }, 500)
        } catch (e: Exception) {
            Log.e("MapView", "Error initializing Maps", e)
        }
    }

    LaunchedEffect(isMapInitialized) {
        if (isMapInitialized) {
            try {
                carIconBitmap = getBitmapDescriptorFromVector(context, R.drawable.ic_car_marker)
                Log.d("MapView", "Car icon created successfully")
            } catch (e: Exception) {
                Log.e("MapView", "Error creating car icon", e)
            }
        }
    }

    LaunchedEffect(currentRoute) {
        if (!initialRouteShown.value && currentRoute != null && currentRoute.points.size > 1) {
            coroutineScope.launch {
                try {
                    val boundsBuilder = LatLngBounds.builder()

                    currentRoute.points.forEach { point ->
                        boundsBuilder.include(LatLng(point.latitude, point.longitude))
                    }

                    destination?.let {
                        boundsBuilder.include(LatLng(it.latitude, it.longitude))
                    }

                    val bounds = boundsBuilder.build()

                    cameraPositionState.animate(
                        update = CameraUpdateFactory.newLatLngBounds(bounds, 100),
                        durationMs = 1000
                    )

                    initialRouteShown.value = true
                } catch (e: Exception) {
                    Log.e("MapView", "Error showing initial route", e)
                    initialRouteShown.value = true
                }
            }
        }
    }

    LaunchedEffect(currentLocation, isDemoMode, followCurrentLocation) {
        if (currentLocation != null && isDemoMode && followCurrentLocation) {
            fixedCarNavigationMode = true

            coroutineScope.launch {
                val position = CameraPosition.Builder()
                    .target(LatLng(currentLocation.latitude, currentLocation.longitude))
                    .zoom(18f)
                    .bearing(currentLocation.bearing)
                    .tilt(60f)
                    .build()

                cameraPositionState.animate(
                    update = CameraUpdateFactory.newCameraPosition(position),
                    durationMs = 500
                )
            }
        } else {
            fixedCarNavigationMode = false
        }
    }

    Box(modifier = modifier) {
        GoogleMap(
            modifier = Modifier.matchParentSize(),
            cameraPositionState = cameraPositionState,
            onMapLongClick = if (!isDemoMode) onMapLongClick else { _ -> },
            uiSettings = MapUiSettings(
                zoomControlsEnabled = !fixedCarNavigationMode,
                scrollGesturesEnabled = !fixedCarNavigationMode,
                zoomGesturesEnabled = !fixedCarNavigationMode,
                rotationGesturesEnabled = !fixedCarNavigationMode,
                tiltGesturesEnabled = !fixedCarNavigationMode,
                compassEnabled = !fixedCarNavigationMode
            ),
            onMapLoaded = {
                isMapLoaded = true
            }
        ) {
            if (isMapLoaded) {
                currentLocation?.let { location ->
                    val position = LatLng(location.latitude, location.longitude)

                    if (fixedCarNavigationMode) {
                        Marker(
                            state = MarkerState(position),
                            title = "Current Location",
                            icon = carIconBitmap ?: BitmapDescriptorFactory.defaultMarker(
                                BitmapDescriptorFactory.HUE_AZURE
                            ),
                            rotation = 0f,
                            anchor = androidx.compose.ui.geometry.Offset(0.5f, 0.5f),
                            flat = true,
                            zIndex = 1f
                        )
                    } else {
                        Marker(
                            state = MarkerState(position),
                            title = "Current Location",
                            icon = carIconBitmap ?: BitmapDescriptorFactory.defaultMarker(
                                BitmapDescriptorFactory.HUE_AZURE
                            ),
                            rotation = location.bearing,
                            anchor = androidx.compose.ui.geometry.Offset(0.5f, 0.5f),
                            flat = true,
                            zIndex = 1f
                        )
                    }
                }

                destination?.let { location ->
                    val position = LatLng(location.latitude, location.longitude)
                    Marker(
                        state = MarkerState(position),
                        title = "Destination"
                    )
                }

                currentRoute?.let { route ->
                    if (route.points.isNotEmpty()) {
                        val routePoints = route.points.map { LatLng(it.latitude, it.longitude) }

                        Polyline(
                            points = routePoints,
                            color = Color.Blue,
                            width = 8f,
                            jointType = JointType.ROUND,
                            startCap = com.google.android.gms.maps.model.RoundCap(),
                            endCap = com.google.android.gms.maps.model.RoundCap(),
                            zIndex = 1f,
                            geodesic = true
                        )

                        Polyline(
                            points = routePoints,
                            color = Color.Cyan.copy(alpha = 0.3f),
                            width = 16f,
                            jointType = JointType.ROUND,
                            startCap = com.google.android.gms.maps.model.RoundCap(),
                            endCap = com.google.android.gms.maps.model.RoundCap(),
                            zIndex = 0f
                        )
                    }
                }

                if (!fixedCarNavigationMode && alternativeRoutes.size > 1) {
                    for (i in 1 until alternativeRoutes.size) {
                        val altRoute = alternativeRoutes[i]
                        if (altRoute.points.isNotEmpty()) {
                            val altRoutePoints = altRoute.points.map { LatLng(it.latitude, it.longitude) }

                            Polyline(
                                points = altRoutePoints,
                                color = Color.Gray,
                                width = 5f,
                                jointType = JointType.ROUND,
                                pattern = listOf(
                                    com.google.android.gms.maps.model.Dash(20f),
                                    com.google.android.gms.maps.model.Gap(10f)
                                ),
                                zIndex = 0f
                            )
                        }
                    }
                }
            }
        }

        if (!isMapLoaded) {
            CircularProgressIndicator(
                modifier = Modifier.align(Alignment.Center)
            )
        }

        if (fixedCarNavigationMode) {
            Box(
                modifier = Modifier
                    .align(Alignment.TopEnd)
                    .size(10.dp)
                    .background(
                        color = Color.Blue,
                        shape = CircleShape
                    )
            )
        }
    }
}

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
