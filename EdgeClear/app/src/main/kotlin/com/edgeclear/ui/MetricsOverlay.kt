package com.edgeclear.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
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
    modifier: Modifier = Modifier
) {
    Card(
        modifier = modifier,
        shape = RoundedCornerShape(8.dp),
        elevation = CardDefaults.cardElevation(defaultElevation = 4.dp)
    ) {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            // Title
            Text(
                text = "Real-Time Metrics",
                style = MaterialTheme.typography.titleLarge,
                modifier = Modifier.padding(bottom = 8.dp)
            )

            // ERLE
            MetricRow(
                label = "ERLE",
                value = String.format("%.1f dB", metrics.erle_dB),
                isWarning = false
            )

            // SI-SDR
            MetricRow(
                label = "SI-SDR Delta",
                value = String.format("%.1f dB", metrics.siSdr_dB),
                isWarning = false
            )

            // CPU (red if >6 ms)
            MetricRow(
                label = "CPU",
                value = String.format("%.1f ms", metrics.cpuMs),
                isWarning = metrics.isCpuBudgetViolated()
            )

            // Latency (red if >40 ms)
            MetricRow(
                label = "Latency",
                value = String.format("%.1f ms", metrics.latencyMs),
                isWarning = metrics.isLatencyBudgetViolated()
            )

            // XRuns
            MetricRow(
                label = "XRuns",
                value = metrics.xrunCount.toString(),
                isWarning = metrics.xrunCount > 0
            )

            Divider()

            // VAD State
            MetricRow(
                label = "VAD",
                value = when (metrics.vadState) {
                    VadState.INACTIVE -> "Inactive"
                    VadState.ACTIVE -> "Active"
                },
                isWarning = false
            )

            // DTD State
            MetricRow(
                label = "DTD",
                value = when (metrics.dtdState) {
                    DtdState.SILENCE -> "Silence"
                    DtdState.NEAR_END_ONLY -> "Near-End"
                    DtdState.FAR_END_ONLY -> "Far-End"
                    DtdState.DOUBLE_TALK -> "Double-Talk"
                },
                isWarning = false
            )

            // AV-VAD Mode
            MetricRow(
                label = "AV-VAD",
                value = metrics.avVadMode,
                isWarning = false
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
        modifier = modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceBetween
    ) {
        Text(
            text = label,
            style = MaterialTheme.typography.bodyLarge
        )
        Text(
            text = value,
            style = MaterialTheme.typography.bodyLarge,
            color = if (isWarning) {
                MaterialTheme.colorScheme.error
            } else {
                MaterialTheme.colorScheme.onSurface
            }
        )
    }
}
