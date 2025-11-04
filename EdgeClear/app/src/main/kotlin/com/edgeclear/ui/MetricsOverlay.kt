package com.edgeclear.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import com.edgeclear.audio.ComponentState
import com.edgeclear.audio.DtdState
import com.edgeclear.audio.MetricsSnapshot
import com.edgeclear.audio.VadState

/**
 * MetricsOverlay: Real-time DSP metrics display
 *
 * Displays:
 * - ERLE (Echo Return Loss Enhancement) in dB
 * - SI-SDR (Signal-to-Distortion Ratio) delta in dB
 * - CPU time per hop (red if >6 ms)
 * - Latency (red if >40 ms)
 * - XRun count
 * - VAD state
 * - DTD state
 *
 * See data-model.md for MetricsSnapshot specification.
 */
@Composable
fun MetricsOverlay(
    metrics: MetricsSnapshot,
    componentState: ComponentState? = null,
    modifier: Modifier = Modifier
) {
    val scrollState = rememberScrollState()

    Card(
        modifier = modifier,
        shape = RoundedCornerShape(12.dp),
        elevation = CardDefaults.cardElevation(defaultElevation = 4.dp)
    ) {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .verticalScroll(scrollState)
                .padding(20.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            // Title
            Text(
                text = "Real-Time Metrics",
                style = MaterialTheme.typography.headlineSmall,
                color = MaterialTheme.colorScheme.primary,
                modifier = Modifier.padding(bottom = 4.dp)
            )

            // Performance Section
            MetricsSection(title = "Performance") {
                MetricRow(
                    label = "CPU per Hop",
                    value = String.format("%.1f ms", metrics.cpuMs),
                    isWarning = metrics.isCpuBudgetViolated()
                )

                MetricRow(
                    label = "End-to-End Latency",
                    value = String.format("%.1f ms", metrics.latencyMs),
                    isWarning = metrics.isLatencyBudgetViolated()
                )

                MetricRow(
                    label = "Buffer XRuns",
                    value = metrics.xrunCount.toString(),
                    isWarning = metrics.xrunCount > 0
                )
            }

            Divider(thickness = 1.dp)

            // Audio Quality Section
            MetricsSection(title = "Audio Quality") {
                MetricRow(
                    label = "ERLE",
                    value = String.format("%.1f dB", metrics.erle_dB),
                    isWarning = false
                )

                MetricRow(
                    label = "SI-SDR Delta",
                    value = String.format("%.1f dB", metrics.siSdr_dB),
                    isWarning = false
                )
            }

            Divider(thickness = 1.dp)

            // Detection States Section
            MetricsSection(title = "Detection States") {
                MetricRow(
                    label = "Voice Activity (VAD)",
                    value = when (metrics.vadState) {
                        VadState.INACTIVE -> "Inactive"
                        VadState.ACTIVE -> "Active"
                    },
                    isWarning = false
                )

                MetricRow(
                    label = "Double-Talk (DTD)",
                    value = when (metrics.dtdState) {
                        DtdState.SILENCE -> "Silence"
                        DtdState.NEAR_END_ONLY -> "Near-End"
                        DtdState.FAR_END_ONLY -> "Far-End"
                        DtdState.DOUBLE_TALK -> "Double-Talk"
                    },
                    isWarning = false
                )

                MetricRow(
                    label = "AV-VAD Mode",
                    value = metrics.avVadMode,
                    isWarning = false
                )
            }

            // Phase 8 T104: Component states
            if (componentState != null) {
                Divider(thickness = 1.dp)

                MetricsSection(title = "Component Status") {
                    ComponentStatusRow(
                        label = "Echo Cancellation (AEC)",
                        enabled = componentState.aecEnabled
                    )

                    ComponentStatusRow(
                        label = "Residual Suppressor (RES)",
                        enabled = componentState.resEnabled
                    )

                    ComponentStatusRow(
                        label = "Denoiser",
                        enabled = componentState.denoiserEnabled
                    )

                    ComponentStatusRow(
                        label = "AV-VAD",
                        enabled = componentState.avVadEnabled,
                        optional = true
                    )
                }
            }
        }
    }
}

@Composable
private fun MetricsSection(
    title: String,
    content: @Composable ColumnScope.() -> Unit
) {
    Column(
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        Text(
            text = title,
            style = MaterialTheme.typography.titleMedium,
            color = MaterialTheme.colorScheme.secondary,
            modifier = Modifier.padding(bottom = 4.dp)
        )
        content()
    }
}

@Composable
private fun ComponentStatusRow(
    label: String,
    enabled: Boolean,
    optional: Boolean = false
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text(
            text = label,
            style = MaterialTheme.typography.bodyLarge,
            modifier = Modifier.weight(1f)
        )
        Surface(
            shape = RoundedCornerShape(12.dp),
            color = if (enabled) {
                MaterialTheme.colorScheme.primaryContainer
            } else if (optional) {
                MaterialTheme.colorScheme.surfaceVariant
            } else {
                MaterialTheme.colorScheme.errorContainer
            },
            modifier = Modifier.padding(start = 8.dp)
        ) {
            Text(
                text = if (enabled) "ON" else "OFF",
                style = MaterialTheme.typography.labelLarge,
                color = if (enabled) {
                    MaterialTheme.colorScheme.onPrimaryContainer
                } else if (optional) {
                    MaterialTheme.colorScheme.onSurfaceVariant
                } else {
                    MaterialTheme.colorScheme.onErrorContainer
                },
                modifier = Modifier.padding(horizontal = 12.dp, vertical = 6.dp)
            )
        }
    }
}

@Composable
private fun MetricRow(
    label: String,
    value: String,
    isWarning: Boolean,
    modifier: Modifier = Modifier
) {
    Row(
        modifier = modifier
            .fillMaxWidth()
            .padding(vertical = 4.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text(
            text = label,
            style = MaterialTheme.typography.bodyLarge,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.weight(1f)
        )
        Text(
            text = value,
            style = MaterialTheme.typography.titleMedium,
            color = if (isWarning) {
                MaterialTheme.colorScheme.error
            } else {
                MaterialTheme.colorScheme.onSurface
            }
        )
    }
}
