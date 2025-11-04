package com.edgeclear.ui.controls

import androidx.compose.foundation.layout.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Done
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.edgeclear.audio.ProcessingPreset
import com.edgeclear.audio.Presets

/**
 * Preset selector dropdown component for EdgeClear audio processing.
 *
 * Displays a dropdown menu with three presets:
 * - Low-latency: Minimal latency (~25 ms)
 * - Quality: Balanced (default, ~38 ms)
 * - Battery saver: Reduced CPU (~35 ms)
 *
 * Shows current preset with target latency and key trade-offs.
 *
 * @param selectedPreset Currently active preset
 * @param onPresetSelected Callback when user selects a preset
 * @param modifier Optional modifier for styling
 * @param enabled Enable/disable preset selection (false during recording)
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun PresetSelector(
    selectedPreset: ProcessingPreset,
    onPresetSelected: (ProcessingPreset) -> Unit,
    modifier: Modifier = Modifier,
    enabled: Boolean = true
) {
    var expanded by remember { mutableStateOf(false) }

    Column(
        modifier = modifier,
        verticalArrangement = Arrangement.spacedBy(4.dp)
    ) {
        Text(
            text = "Processing Preset",
            style = MaterialTheme.typography.labelMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant
        )

        ExposedDropdownMenuBox(
            expanded = expanded,
            onExpandedChange = { expanded = !expanded && enabled }
        ) {
            OutlinedTextField(
                value = selectedPreset.name,
                onValueChange = { },
                readOnly = true,
                enabled = enabled,
                label = { Text("Preset") },
                trailingIcon = {
                    ExposedDropdownMenuDefaults.TrailingIcon(expanded = expanded)
                },
                colors = ExposedDropdownMenuDefaults.outlinedTextFieldColors(),
                modifier = Modifier
                    .menuAnchor()
                    .fillMaxWidth(),
                supportingText = {
                    Text(
                        text = buildString {
                            append("Target: ${selectedPreset.targetLatencyMs} ms")
                            if (!selectedPreset.resEnabled) append(" • RES off")
                            if (selectedPreset.denoiserFrameRate > 1) append(" • Denoiser /2")
                        },
                        style = MaterialTheme.typography.bodySmall
                    )
                }
            )

            ExposedDropdownMenu(
                expanded = expanded,
                onDismissRequest = { expanded = false }
            ) {
                Presets.ALL.forEach { preset ->
                    DropdownMenuItem(
                        text = {
                            Column {
                                Text(
                                    text = preset.name,
                                    style = MaterialTheme.typography.bodyLarge
                                )
                                Text(
                                    text = buildPresetDescription(preset),
                                    style = MaterialTheme.typography.bodySmall,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant
                                )
                            }
                        },
                        onClick = {
                            onPresetSelected(preset)
                            expanded = false
                        },
                        leadingIcon = if (preset == selectedPreset) {
                            {
                                Icon(
                                    imageVector = Icons.Filled.Done,
                                    contentDescription = "Selected",
                                    tint = MaterialTheme.colorScheme.primary
                                )
                            }
                        } else null
                    )
                }
            }
        }

        // Preset details card
        if (selectedPreset == Presets.LOW_LATENCY) {
            InfoCard(
                title = "Low Latency Mode",
                description = "Minimal buffering and shorter AEC filter for fastest response. " +
                        "May increase XRun risk on busy systems.",
                color = MaterialTheme.colorScheme.primaryContainer
            )
        } else if (selectedPreset == Presets.BATTERY_SAVER) {
            InfoCard(
                title = "Battery Saver Mode",
                description = "Reduced CPU usage (~1.7 ms savings) through denoiser decimation. " +
                        "Lower camera frame rate conserves power.",
                color = MaterialTheme.colorScheme.tertiaryContainer
            )
        }
    }
}

/**
 * Build human-readable description for preset dropdown.
 */
private fun buildPresetDescription(preset: ProcessingPreset): String {
    return buildString {
        append("${preset.targetLatencyMs} ms latency")

        val features = mutableListOf<String>()
        if (!preset.resEnabled) features.add("RES off")
        if (preset.denoiserFrameRate > 1) features.add("Denoiser /2")
        if (preset.bufferHops == 1) features.add("tight buffers")
        if (preset.aecFilterLength == 4) features.add("short AEC")

        if (features.isNotEmpty()) {
            append(" (")
            append(features.joinToString(", "))
            append(")")
        }
    }
}

/**
 * Info card for preset trade-offs.
 */
@Composable
private fun InfoCard(
    title: String,
    description: String,
    color: androidx.compose.ui.graphics.Color,
    modifier: Modifier = Modifier
) {
    Card(
        modifier = modifier.fillMaxWidth(),
        colors = CardDefaults.cardColors(containerColor = color)
    ) {
        Column(
            modifier = Modifier.padding(12.dp),
            verticalArrangement = Arrangement.spacedBy(4.dp)
        ) {
            Text(
                text = title,
                style = MaterialTheme.typography.labelLarge
            )
            Text(
                text = description,
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }
    }
}
