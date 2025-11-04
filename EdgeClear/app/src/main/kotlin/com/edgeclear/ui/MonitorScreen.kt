package com.edgeclear.ui

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.edgeclear.audio.ProcessingPreset
import com.edgeclear.viewmodel.MonitorViewModel

/**
 * MonitorScreen: Main monitoring UI
 *
 * Displays:
 * - Start/Stop monitoring button
 * - Real-time metrics overlay
 * - Current preset
 * - Error messages
 *
 * See spec.md User Story 1 for requirements.
 */
@Composable
fun MonitorScreen(
    viewModel: MonitorViewModel = viewModel()
) {
    val isMonitoring by viewModel.isMonitoring.collectAsState()
    val metrics by viewModel.metrics.collectAsState()
    val session by viewModel.session.collectAsState()
    val errorMessage by viewModel.errorMessage.collectAsState()

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        // Header
        Text(
            text = "EdgeClear Monitor",
            style = MaterialTheme.typography.headlineMedium,
            modifier = Modifier.padding(bottom = 32.dp)
        )

        // Metrics overlay (if monitoring)
        if (isMonitoring && metrics != null) {
            MetricsOverlay(
                metrics = metrics!!,
                modifier = Modifier
                    .fillMaxWidth()
                    .weight(1f)
                    .padding(bottom = 16.dp)
            )
        } else {
            Spacer(modifier = Modifier.weight(1f))
        }

        // Current preset display
        if (session != null) {
            Text(
                text = "Preset: ${session!!.currentPreset.displayName}",
                style = MaterialTheme.typography.bodyLarge,
                modifier = Modifier.padding(bottom = 8.dp)
            )
        }

        // Phase 8 TODO: Component toggles will be added here (AEC, RES, Denoiser, AV-VAD)
        // Phase 6 T081: When toggles are added, implement lock check:
        //   val isRecordingActive by viewModel.isRecordingActive.collectAsState()
        //   val snackbarHostState = remember { SnackbarHostState() }
        //
        //   In each toggle onClick:
        //     if (isRecordingActive) {
        //         scope.launch {
        //             snackbarHostState.showSnackbar(
        //                 message = "Cannot change components during recording",
        //                 duration = SnackbarDuration.Short
        //             )
        //         }
        //     } else {
        //         // Allow toggle
        //     }

        // Error message
        if (errorMessage != null) {
            Text(
                text = errorMessage!!,
                color = MaterialTheme.colorScheme.error,
                style = MaterialTheme.typography.bodyMedium,
                modifier = Modifier.padding(bottom = 16.dp)
            )
        }

        // Start/Stop button
        Button(
            onClick = {
                if (isMonitoring) {
                    viewModel.stopMonitoring()
                } else {
                    viewModel.startMonitoring(ProcessingPreset.QUALITY)
                }
            },
            modifier = Modifier
                .fillMaxWidth()
                .height(56.dp)
        ) {
            Text(
                text = if (isMonitoring) "Stop Monitoring" else "Start Monitoring",
                style = MaterialTheme.typography.titleMedium
            )
        }
    }
}
