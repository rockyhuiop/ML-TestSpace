package com.edgeclear.ui

import android.content.Context
import android.content.Intent
import android.media.MediaPlayer
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Share
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.core.content.FileProvider
import com.edgeclear.audio.ABRecording
import java.io.File
import java.time.format.DateTimeFormatter

/**
 * RecordingLibrary: Displays list of saved A/B recordings with playback/share/delete controls
 *
 * Features:
 * - List recordings with timestamp, duration, size
 * - Play/pause/stop controls for each file (raw, far-end, enhanced)
 * - Share recording via Android share sheet
 * - Delete recording
 *
 * See spec.md User Story 2 for requirements.
 */
@Composable
fun RecordingLibrary(
    recordings: List<ABRecording>,
    onDeleteRecording: (ABRecording) -> Unit,
    modifier: Modifier = Modifier
) {
    val context = LocalContext.current

    Card(
        modifier = modifier.fillMaxWidth()
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(16.dp)
        ) {
            Text(
                text = "Recording Library",
                style = MaterialTheme.typography.titleLarge,
                modifier = Modifier.padding(bottom = 16.dp)
            )

            if (recordings.isEmpty()) {
                Text(
                    text = "No recordings yet",
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            } else {
                LazyColumn(
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    items(recordings) { recording ->
                        RecordingItem(
                            recording = recording,
                            onPlay = { file ->
                                playAudioFile(context, file)
                            },
                            onShare = {
                                shareRecording(context, recording)
                            },
                            onDelete = {
                                onDeleteRecording(recording)
                            }
                        )
                    }
                }
            }
        }
    }
}

/**
 * Individual recording item with controls
 */
@Composable
fun RecordingItem(
    recording: ABRecording,
    onPlay: (String) -> Unit,
    onShare: () -> Unit,
    onDelete: () -> Unit
) {
    var expanded by remember { mutableStateOf(false) }
    var currentlyPlaying by remember { mutableStateOf<String?>(null) }

    Card(
        modifier = Modifier.fillMaxWidth()
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .clickable { expanded = !expanded }
                .padding(16.dp)
        ) {
            // Header row
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Column(modifier = Modifier.weight(1f)) {
                    Text(
                        text = recording.displayName,
                        style = MaterialTheme.typography.titleSmall
                    )
                    Text(
                        text = formatTimestamp(recording.timestamp),
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                    Text(
                        text = "${recording.durationSeconds}s • ${String.format("%.1f MB", recording.getSizeMB())}",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }

                // Action buttons
                Row(
                    horizontalArrangement = Arrangement.spacedBy(4.dp)
                ) {
                    IconButton(onClick = onShare) {
                        Icon(Icons.Default.Share, contentDescription = "Share")
                    }
                    IconButton(onClick = onDelete) {
                        Icon(
                            Icons.Default.Delete,
                            contentDescription = "Delete",
                            tint = MaterialTheme.colorScheme.error
                        )
                    }
                }
            }

            // Expanded playback controls
            if (expanded) {
                Divider(modifier = Modifier.padding(vertical = 8.dp))

                Column(
                    verticalArrangement = Arrangement.spacedBy(4.dp)
                ) {
                    AudioFileControl(
                        label = "Raw Microphone",
                        filePath = recording.rawFilePath,
                        isPlaying = currentlyPlaying == recording.rawFilePath,
                        onPlay = {
                            onPlay(recording.rawFilePath)
                            currentlyPlaying = recording.rawFilePath
                        },
                        onStop = {
                            currentlyPlaying = null
                        }
                    )

                    AudioFileControl(
                        label = "Far-End Reference",
                        filePath = recording.farEndFilePath,
                        isPlaying = currentlyPlaying == recording.farEndFilePath,
                        onPlay = {
                            onPlay(recording.farEndFilePath)
                            currentlyPlaying = recording.farEndFilePath
                        },
                        onStop = {
                            currentlyPlaying = null
                        }
                    )

                    AudioFileControl(
                        label = "Enhanced Output",
                        filePath = recording.enhancedFilePath,
                        isPlaying = currentlyPlaying == recording.enhancedFilePath,
                        onPlay = {
                            onPlay(recording.enhancedFilePath)
                            currentlyPlaying = recording.enhancedFilePath
                        },
                        onStop = {
                            currentlyPlaying = null
                        }
                    )
                }
            }
        }
    }
}

/**
 * Audio file playback control row
 */
@Composable
fun AudioFileControl(
    label: String,
    filePath: String,
    isPlaying: Boolean,
    onPlay: () -> Unit,
    onStop: () -> Unit
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text(
            text = label,
            style = MaterialTheme.typography.bodyMedium,
            modifier = Modifier.weight(1f)
        )

        IconButton(
            onClick = {
                if (isPlaying) {
                    onStop()
                } else {
                    onPlay()
                }
            }
        ) {
            Icon(
                imageVector = if (isPlaying) Icons.Default.Close else Icons.Default.PlayArrow,
                contentDescription = if (isPlaying) "Stop" else "Play"
            )
        }
    }
}

/**
 * Simple media player for WAV playback
 *
 * Note: This is a basic implementation. For production, use a proper MediaPlayer
 * manager with lifecycle handling and proper resource cleanup.
 */
private fun playAudioFile(context: Context, filePath: String) {
    try {
        val mediaPlayer = MediaPlayer()
        mediaPlayer.setDataSource(filePath)
        mediaPlayer.prepare()
        mediaPlayer.start()

        // Release player when done
        mediaPlayer.setOnCompletionListener {
            it.release()
        }
    } catch (e: Exception) {
        e.printStackTrace()
        // TODO: Show error toast
    }
}

/**
 * Shares recording files via Android share sheet
 *
 * Creates file URIs using FileProvider and launches share intent with all 4 files:
 * - raw.wav
 * - far_end.wav
 * - enhanced.wav
 * - metadata.json
 */
private fun shareRecording(context: Context, recording: ABRecording) {
    try {
        val files = recording.getAllFilePaths()
            .map { File(it) }
            .filter { it.exists() }

        if (files.isEmpty()) {
            // TODO: Show error toast
            return
        }

        // Create file URIs using FileProvider
        val uris = files.map { file ->
            FileProvider.getUriForFile(
                context,
                "${context.packageName}.fileprovider",
                file
            )
        }

        // Create share intent
        val shareIntent = Intent(Intent.ACTION_SEND_MULTIPLE).apply {
            type = "*/*"
            putParcelableArrayListExtra(Intent.EXTRA_STREAM, ArrayList(uris))
            putExtra(Intent.EXTRA_SUBJECT, "EdgeClear A/B Recording: ${recording.displayName}")
            putExtra(
                Intent.EXTRA_TEXT,
                "Recording from ${formatTimestamp(recording.timestamp)}\n" +
                        "Duration: ${recording.durationSeconds}s\n" +
                        "Files: raw.wav, far_end.wav, enhanced.wav, metadata.json"
            )
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }

        context.startActivity(Intent.createChooser(shareIntent, "Share Recording"))
    } catch (e: Exception) {
        e.printStackTrace()
        // TODO: Show error toast
    }
}

/**
 * Formats Instant timestamp to human-readable string
 */
private fun formatTimestamp(instant: java.time.Instant): String {
    return try {
        val formatter = DateTimeFormatter.ofPattern("MMM dd, yyyy HH:mm:ss")
        val localDateTime = java.time.LocalDateTime.ofInstant(
            instant,
            java.time.ZoneId.systemDefault()
        )
        localDateTime.format(formatter)
    } catch (e: Exception) {
        instant.toString()
    }
}
