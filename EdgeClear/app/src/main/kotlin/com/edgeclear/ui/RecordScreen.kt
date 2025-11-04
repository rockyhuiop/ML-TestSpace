package com.edgeclear.ui

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.edgeclear.audio.ComponentState
import com.edgeclear.viewmodel.RecordViewModel
import com.edgeclear.viewmodel.RecordingState

/**
 * RecordScreen: A/B Recording UI
 *
 * Displays:
 * - Recording controls (start/stop, duration selector)
 * - Recording timer with progress
 * - Storage availability info
 * - Recording library
 * - Error messages
 *
 * See spec.md User Story 2 for requirements.
 */
@Composable
fun RecordScreen(
    viewModel: RecordViewModel = viewModel(),
    componentStates: ComponentState
) {
    val recordingState by viewModel.recordingState.collectAsState()
    val currentRecording by viewModel.currentRecording.collectAsState()
    val elapsedSeconds by viewModel.elapsedSeconds.collectAsState()
    val selectedDuration by viewModel.selectedDuration.collectAsState()
    val recordings by viewModel.recordings.collectAsState()
    val errorMessage by viewModel.errorMessage.collectAsState()
    val storageInfo by viewModel.storageInfo.collectAsState()

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        // Header
        Text(
            text = "A/B Recording",
            style = MaterialTheme.typography.headlineMedium,
            modifier = Modifier.padding(bottom = 16.dp)
        )

        // Storage info
        Text(
            text = storageInfo,
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.padding(bottom = 16.dp)
        )

        // Recording status
        when (recordingState) {
            is RecordingState.Recording -> {
                RecordingStatusCard(
                    elapsedSeconds = elapsedSeconds,
                    totalSeconds = selectedDuration,
                    modifier = Modifier.padding(bottom = 16.dp)
                )
            }
            is RecordingState.Completed -> {
                Text(
                    text = "Recording completed!",
                    color = MaterialTheme.colorScheme.primary,
                    style = MaterialTheme.typography.titleMedium,
                    modifier = Modifier.padding(bottom = 16.dp)
                )
            }
            is RecordingState.Error -> {
                Text(
                    text = (recordingState as RecordingState.Error).message,
                    color = MaterialTheme.colorScheme.error,
                    style = MaterialTheme.typography.bodyMedium,
                    modifier = Modifier.padding(bottom = 16.dp)
                )
            }
            else -> {
                // Idle - show duration selector
                DurationSelector(
                    selectedDuration = selectedDuration,
                    onDurationSelected = { viewModel.setDuration(it) },
                    enabled = recordingState == RecordingState.Idle,
                    modifier = Modifier.padding(bottom = 16.dp)
                )
            }
        }

        // Error message
        if (errorMessage != null) {
            Card(
                colors = CardDefaults.cardColors(
                    containerColor = MaterialTheme.colorScheme.errorContainer
                ),
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(bottom = 16.dp)
            ) {
                Text(
                    text = errorMessage!!,
                    color = MaterialTheme.colorScheme.onErrorContainer,
                    style = MaterialTheme.typography.bodyMedium,
                    modifier = Modifier.padding(16.dp)
                )
            }

            Button(
                onClick = { viewModel.clearError() },
                modifier = Modifier.padding(bottom = 16.dp)
            ) {
                Text("Dismiss")
            }
        }

        Spacer(modifier = Modifier.weight(1f))

        // Recording controls
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(bottom = 16.dp),
            horizontalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            // Start/Stop button
            Button(
                onClick = {
                    when (recordingState) {
                        RecordingState.Recording -> viewModel.stopRecording()
                        RecordingState.Idle -> viewModel.startRecording(componentStates)
                        else -> {}
                    }
                },
                enabled = recordingState != RecordingState.Completed,
                modifier = Modifier
                    .weight(1f)
                    .height(56.dp)
            ) {
                Text(
                    text = when (recordingState) {
                        RecordingState.Recording -> "Stop Recording"
                        else -> "Start Recording"
                    },
                    style = MaterialTheme.typography.titleMedium
                )
            }
        }

        // Recording library preview
        if (recordings.isNotEmpty()) {
            Text(
                text = "Recent Recordings (${recordings.size})",
                style = MaterialTheme.typography.titleSmall,
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(bottom = 8.dp)
            )
        }
    }
}

/**
 * Recording status card showing progress
 */
@Composable
fun RecordingStatusCard(
    elapsedSeconds: Int,
    totalSeconds: Int,
    modifier: Modifier = Modifier
) {
    Card(
        modifier = modifier.fillMaxWidth(),
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.primaryContainer
        )
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(16.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Text(
                text = "Recording in progress...",
                style = MaterialTheme.typography.titleMedium,
                color = MaterialTheme.colorScheme.onPrimaryContainer,
                modifier = Modifier.padding(bottom = 8.dp)
            )

            Text(
                text = formatTime(elapsedSeconds) + " / " + formatTime(totalSeconds),
                style = MaterialTheme.typography.headlineSmall,
                color = MaterialTheme.colorScheme.onPrimaryContainer,
                modifier = Modifier.padding(bottom = 8.dp)
            )

            LinearProgressIndicator(
                progress = elapsedSeconds.toFloat() / totalSeconds.toFloat(),
                modifier = Modifier
                    .fillMaxWidth()
                    .height(8.dp)
            )
        }
    }
}

/**
 * Duration selector (15, 30, 60 seconds)
 */
@Composable
fun DurationSelector(
    selectedDuration: Int,
    onDurationSelected: (Int) -> Unit,
    enabled: Boolean,
    modifier: Modifier = Modifier
) {
    Column(
        modifier = modifier.fillMaxWidth()
    ) {
        Text(
            text = "Recording Duration",
            style = MaterialTheme.typography.titleSmall,
            modifier = Modifier.padding(bottom = 8.dp)
        )

        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            DurationChip(
                duration = 15,
                isSelected = selectedDuration == 15,
                onClick = { onDurationSelected(15) },
                enabled = enabled,
                modifier = Modifier.weight(1f)
            )
            DurationChip(
                duration = 30,
                isSelected = selectedDuration == 30,
                onClick = { onDurationSelected(30) },
                enabled = enabled,
                modifier = Modifier.weight(1f)
            )
            DurationChip(
                duration = 60,
                isSelected = selectedDuration == 60,
                onClick = { onDurationSelected(60) },
                enabled = enabled,
                modifier = Modifier.weight(1f)
            )
        }
    }
}

/**
 * Individual duration chip (selectable button)
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun DurationChip(
    duration: Int,
    isSelected: Boolean,
    onClick: () -> Unit,
    enabled: Boolean,
    modifier: Modifier = Modifier
) {
    FilterChip(
        selected = isSelected,
        onClick = onClick,
        enabled = enabled,
        label = {
            Text(
                text = "${duration}s",
                style = MaterialTheme.typography.bodyMedium
            )
        },
        modifier = modifier
    )
}

/**
 * Formats seconds to MM:SS
 */
private fun formatTime(seconds: Int): String {
    val mins = seconds / 60
    val secs = seconds % 60
    return String.format("%02d:%02d", mins, secs)
}
