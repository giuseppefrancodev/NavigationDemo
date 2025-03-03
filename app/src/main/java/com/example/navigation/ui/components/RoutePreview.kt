/*
 * File: RoutePreview.kt
 * Description: Composable UI component for previewing and selecting navigation routes, including alternative routes.
 * Author: Giuseppe Franco
 * Created: March 2025
 */

package com.example.navigation.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.example.navigation.domain.models.Route

@Composable
fun RoutePreview(
    currentRoute: Route?,
    alternativeRoutes: List<Route>,
    onRouteSelected: (String) -> Unit,
    onStartNavigation: () -> Unit,
    modifier: Modifier = Modifier
) {
    if (currentRoute == null) return

    Card(
        modifier = modifier
            .fillMaxWidth()
            .height(260.dp),
        shape = RoundedCornerShape(16.dp),
        elevation = CardDefaults.cardElevation(defaultElevation = 8.dp)
    ) {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(16.dp)
        ) {
            Text(
                text = "Route to ${currentRoute.name}",
                style = MaterialTheme.typography.headlineSmall,
                fontWeight = FontWeight.Bold,
                color = MaterialTheme.colorScheme.onSurface
            )

            Spacer(modifier = Modifier.height(8.dp))

            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                RouteDetailItem(
                    label = "Distance",
                    value = currentRoute.distanceFormatted
                )
                RouteDetailItem(
                    label = "Duration",
                    value = currentRoute.durationFormatted
                )
            }

            Spacer(modifier = Modifier.height(16.dp))

            if (alternativeRoutes.size > 1) {
                Text(
                    text = "Alternative Routes",
                    style = MaterialTheme.typography.bodyMedium,
                    fontWeight = FontWeight.Medium,
                    color = MaterialTheme.colorScheme.onSurface
                )

                Spacer(modifier = Modifier.height(8.dp))

                LazyRow(
                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    items(alternativeRoutes) { route ->
                        RouteOption(
                            route = route,
                            isSelected = route.id == currentRoute.id,
                            onClick = { onRouteSelected(route.id) }
                        )
                    }
                }

                Spacer(modifier = Modifier.height(8.dp))
            }

            Spacer(modifier = Modifier.weight(1f))

            Button(
                onClick = onStartNavigation,
                modifier = Modifier
                    .fillMaxWidth()
                    .height(56.dp),
                colors = ButtonDefaults.buttonColors(
                    containerColor = MaterialTheme.colorScheme.primary,
                    contentColor = MaterialTheme.colorScheme.onPrimary
                ),
                shape = RoundedCornerShape(12.dp)
            ) {
                Text(
                    text = "Start Navigation",
                    style = MaterialTheme.typography.bodyLarge,
                    fontWeight = FontWeight.Bold
                )
            }
        }
    }
}

@Composable
fun RouteOption(route: Route, isSelected: Boolean, onClick: () -> Unit) {
    Box(
        modifier = Modifier
            .width(100.dp)
            .height(40.dp)
            .clip(RoundedCornerShape(8.dp))
            .background(
                if (isSelected) {
                    MaterialTheme.colorScheme.primary
                } else {
                    MaterialTheme.colorScheme.surfaceVariant
                }
            )
            .clickable(onClick = onClick)
            .padding(8.dp),
        contentAlignment = Alignment.Center
    ) {
        Text(
            text = route.durationFormatted,
            color = if (isSelected) {
                MaterialTheme.colorScheme.onPrimary
            } else {
                MaterialTheme.colorScheme.onSurfaceVariant
            },
            style = MaterialTheme.typography.bodyMedium,
            fontWeight = FontWeight.Medium
        )
    }
}

@Composable
fun RouteDetailItem(label: String, value: String) {
    Column(
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Text(
            text = label,
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant
        )
        Spacer(modifier = Modifier.height(4.dp))
        Text(
            text = value,
            style = MaterialTheme.typography.bodyLarge,
            fontWeight = FontWeight.Bold,
            color = MaterialTheme.colorScheme.onSurface
        )
    }
}
