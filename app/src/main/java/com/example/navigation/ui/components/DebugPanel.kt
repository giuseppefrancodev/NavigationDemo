package com.example.navigation.ui.components

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.expandHorizontally
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkHorizontally
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Info
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.FloatingActionButton
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.rotate
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import com.example.navigation.viewmodels.NavigationState
import com.example.navigation.viewmodels.NavigationViewModel

/**
 * Debug panel for showing navigation state and providing testing controls.
 */
@Composable
fun DebugPanel(
    state: NavigationState,
    viewModel: NavigationViewModel,
    onClose: () -> Unit,
    modifier: Modifier = Modifier
) {
    // Create a state to track expansion - we'll start collapsed
    androidx.compose.runtime.remember { androidx.compose.runtime.mutableStateOf(false) }.let { expansionState ->
        val isExpanded = expansionState.value

        // Call our actual implementation with the conversion from old to new parameters
        ExpandableDebugPanel(
            state = state,
            viewModel = viewModel,
            isExpanded = isExpanded,
            onToggleExpanded = {
                expansionState.value = !expansionState.value
                // Also call onClose when collapsing to maintain compatibility
                if (!expansionState.value) onClose()
            },
            modifier = modifier
        )
    }
}

/**
 * Animated unified Debug Panel that expands to show simulation controls.
 * This is the actual implementation with the new parameters.
 */
@Composable
private fun ExpandableDebugPanel(
    state: NavigationState,
    viewModel: NavigationViewModel,
    isExpanded: Boolean,
    onToggleExpanded: () -> Unit,
    modifier: Modifier = Modifier
) {
    val isDemoMode by viewModel.isDemoMode.collectAsState()

    // Animation for rotating the info icon
    val iconRotation by animateFloatAsState(
        targetValue = if (isExpanded) 45f else 0f,
        label = "iconRotation"
    )

    // Base surface that holds everything
    Surface(
        shape = RoundedCornerShape(24.dp),
        color = MaterialTheme.colorScheme.surface.copy(alpha = 0.9f),
        tonalElevation = 2.dp,
        shadowElevation = 4.dp,
        modifier = modifier
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(8.dp),
            modifier = Modifier.padding(4.dp)
        ) {
            // Use FloatingActionButton instead of Button for more reliable icon display
            FloatingActionButton(
                onClick = onToggleExpanded,
                modifier = Modifier.size(40.dp),
                containerColor = Color.Transparent
            ) {
                Icon(
                    imageVector = Icons.Filled.Info,
                    contentDescription = "Debug Controls",
                    modifier = Modifier.rotate(iconRotation),
                    tint = Color.White // Force white color for better visibility
                )
            }

            // Expandable section with buttons
            AnimatedVisibility(
                visible = isExpanded,
                enter = expandHorizontally(expandFrom = Alignment.Start) + fadeIn(),
                exit = shrinkHorizontally(shrinkTowards = Alignment.Start) + fadeOut()
            ) {
                Row(
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    // Simulation button
                    Button(
                        onClick = {
                            if (isDemoMode) {
                                viewModel.stopSimulation()
                            } else {
                                viewModel.startSimulation()
                            }
                        },
                        enabled = isDemoMode || state.currentRoute != null,
                        colors = ButtonDefaults.buttonColors(
                            containerColor = if (isDemoMode)
                                MaterialTheme.colorScheme.error
                            else
                                MaterialTheme.colorScheme.primary
                        ),
                        shape = RoundedCornerShape(20.dp),
                        modifier = Modifier.size(width = 110.dp, height = 36.dp)
                    ) {
                        Text(if (isDemoMode) "Stop Sim" else "Start Sim")
                    }

                    // Reset button
                    Button(
                        onClick = {
                            // Stop any ongoing navigation or simulation
                            if (isDemoMode) {
                                viewModel.stopSimulation()
                            } else if (state.isNavigating) {
                                viewModel.stopNavigation()
                            }

                            // Clear destination if any was set
                            if (state.destination != null) {
                                viewModel.clearDestination()
                            }
                        },
                        colors = ButtonDefaults.buttonColors(
                            containerColor = MaterialTheme.colorScheme.secondary
                        ),
                        shape = RoundedCornerShape(20.dp),
                        modifier = Modifier.size(width = 84.dp, height = 36.dp)
                    ) {
                        Text("Reset")
                    }
                }
            }
        }
    }
}