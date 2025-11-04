package com.edgeclear.ui

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.edgeclear.audio.Presets
import com.edgeclear.viewmodel.MonitorViewModel
import com.edgeclear.ui.controls.PresetSelector
import com.edgeclear.ui.controls.ComponentToggle
import kotlinx.coroutines.launch
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
    val snackbarHostState = remember { SnackbarHostState() }
    val scope = rememberCoroutineScope()

    Scaffold(
        snackbarHost = { SnackbarHost(snackbarHostState) }
    ) { paddingValues ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(paddingValues)
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
                componentState = session?.componentState,
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
                text = "Preset: ${session!!.currentPreset.name}",
                style = MaterialTheme.typography.bodyLarge,
                modifier = Modifier.padding(bottom = 8.dp)
            )
        }

        // Component toggles (Phase 8: T097)
        if (session != null) {
            val isRecordingActive by viewModel.isRecordingActive.collectAsState()
            val componentState = session!!.componentState

            Card(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(bottom = 16.dp),
                elevation = CardDefaults.cardElevation(defaultElevation = 2.dp)
            ) {
                Column(
                    modifier = Modifier.padding(16.dp)
                ) {
                    Text(
                        text = "DSP Components",
                        style = MaterialTheme.typography.titleMedium,
                        modifier = Modifier.padding(bottom = 8.dp)
                    )

                    // AEC Toggle
                    ComponentToggle(
                        label = "AEC (Echo Cancellation)",
                        checked = componentState.aecEnabled,
                        onCheckedChange = { enabled ->
                            if (isRecordingActive) {
                                scope.launch {
                                    snackbarHostState.showSnackbar(
                                        message = "Cannot change components during recording",
                                        duration = SnackbarDuration.Short
                                    )
                                }
                            } else {
                                viewModel.toggleComponent("aec", enabled)
                            }
                        },
                        enabled = isMonitoring && !isRecordingActive
                    )

                    // RES Toggle
                    ComponentToggle(
                        label = "RES (Residual Echo Suppressor)",
                        checked = componentState.resEnabled,
                        onCheckedChange = { enabled ->
                            if (isRecordingActive) {
                                scope.launch {
                                    snackbarHostState.showSnackbar(
                                        message = "Cannot change components during recording",
                                        duration = SnackbarDuration.Short
                                    )
                                }
                            } else {
                                viewModel.toggleComponent("res", enabled)
                            }
                        },
                        enabled = isMonitoring && !isRecordingActive
                    )

                    // Denoiser Toggle
                    ComponentToggle(
                        label = "Denoiser",
                        checked = componentState.denoiserEnabled,
                        onCheckedChange = { enabled ->
                            if (isRecordingActive) {
                                scope.launch {
                                    snackbarHostState.showSnackbar(
                                        message = "Cannot change components during recording",
                                        duration = SnackbarDuration.Short
                                    )
                                }
                            } else {
                                viewModel.toggleComponent("denoiser", enabled)
                            }
                        },
                        enabled = isMonitoring && !isRecordingActive
                    )

                    // AV-VAD Toggle
                    ComponentToggle(
                        label = "AV-VAD (Audio-Visual VAD)",
                        checked = componentState.avVadEnabled,
                        onCheckedChange = { enabled ->
                            if (isRecordingActive) {
                                scope.launch {
                                    snackbarHostState.showSnackbar(
                                        message = "Cannot change components during recording",
                                        duration = SnackbarDuration.Short
                                    )
                                }
                            } else {
                                viewModel.toggleComponent("av_vad", enabled)
                            }
                        },
                        enabled = isMonitoring && !isRecordingActive
                    )
                }
            }
        }

        // Phase 7 T095: Preset selector (disabled during recording)
        if (session != null) {
            val isRecordingActive by viewModel.isRecordingActive.collectAsState()
            
            PresetSelector(
                selectedPreset = session!!.currentPreset,
                onPresetSelected = { preset ->
                    viewModel.changePreset(preset)
                },
                enabled = isMonitoring && !isRecordingActive,
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(bottom = 16.dp)
            )
        }


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
                    viewModel.startMonitoring(Presets.DEFAULT)
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
}
