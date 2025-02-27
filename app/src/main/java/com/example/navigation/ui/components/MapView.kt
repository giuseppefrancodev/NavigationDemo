package com.example.navigation.ui.components

import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import com.example.navigation.domain.models.Location
import com.example.navigation.domain.models.Route
import com.google.android.gms.maps.model.LatLng
import com.google.android.gms.maps.model.MapStyleOptions
import com.google.maps.android.compose.*
import com.example.navigation.R

@Composable
fun MapView(
    modifier: Modifier = Modifier,
    cameraPositionState: CameraPositionState,
    currentLocation: Location?,
    destination: Location?,
    currentRoute: Route?,
    alternativeRoutes: List<Route>,
    onMapLongClick: (LatLng) -> Unit
) {
    GoogleMap(
        modifier = modifier,
        cameraPositionState = cameraPositionState,
        properties = MapProperties(
            isMyLocationEnabled = currentLocation != null,
            mapStyleOptions = MapStyleOptions.loadRawResourceStyle(
                androidx.compose.ui.platform.LocalContext.current,
                R.raw.google_map_style
            )
        ),
        uiSettings = MapUiSettings(
            zoomControlsEnabled = false,
            myLocationButtonEnabled = false,
            mapToolbarEnabled = false
        ),
        onMapLongClick = onMapLongClick
    ) {
        // Show alternative routes
        alternativeRoutes.forEach { route ->
            if (route.id != currentRoute?.id) {
                Polyline(
                    points = route.points.map { LatLng(it.latitude, it.longitude) },
                    color = Color.Gray,
                    width = 5f,
                    zIndex = 1f
                )
            }
        }

        // Show current route
        currentRoute?.let { route ->
            Polyline(
                points = route.points.map { LatLng(it.latitude, it.longitude) },
                color = Color.Blue,
                width = 8f,
                zIndex = 2f
            )
        }

        // Show destination marker
        destination?.let { dest ->
            Marker(
                state = MarkerState(position = LatLng(dest.latitude, dest.longitude)),
                title = "Destination"
            )
        }
    }
}