package com.edgeclear.audio

import java.io.File
import java.time.Instant

/**
 * Represents an A/B recording session with three synchronized audio files:
 * - Raw microphone input
 * - Far-end reference audio
 * - Enhanced output after DSP processing
 *
 * Used by RecordViewModel to track and manage recorded audio sessions.
 */
data class ABRecording(
    /** Unique identifier for this recording session */
    val sessionId: String,

    /** Timestamp when recording started */
    val timestamp: Instant,

    /** Duration of the recording in seconds */
    val durationSeconds: Int,

    /** Path to raw microphone input WAV file (48 kHz, 16-bit PCM) */
    val rawFilePath: String,

    /** Path to far-end reference WAV file (48 kHz, 16-bit PCM) */
    val farEndFilePath: String,

    /** Path to enhanced output WAV file (48 kHz, 16-bit PCM) */
    val enhancedFilePath: String,

    /** Path to metadata JSON file containing session configuration */
    val metadataFilePath: String,

    /** Snapshot of component states during recording (for metadata integrity) */
    val componentStates: ComponentState,

    /** Display name for the recording (user-friendly) */
    val displayName: String = "Recording_${timestamp.epochSecond}",

    /** Total size of all files in bytes */
    val totalSizeBytes: Long = 0L
) {
    /**
     * Checks if all expected files exist on disk
     */
    fun filesExist(): Boolean {
        return listOf(rawFilePath, farEndFilePath, enhancedFilePath, metadataFilePath)
            .all { path -> File(path).exists() }
    }

    /**
     * Deletes all files associated with this recording
     */
    fun deleteFiles(): Boolean {
        return listOf(rawFilePath, farEndFilePath, enhancedFilePath, metadataFilePath)
            .all { path -> File(path).delete() }
    }

    /**
     * Returns list of all file paths for sharing
     */
    fun getAllFilePaths(): List<String> {
        return listOf(rawFilePath, farEndFilePath, enhancedFilePath, metadataFilePath)
    }

    /**
     * Returns estimated storage size in MB
     */
    fun getSizeMB(): Double {
        return totalSizeBytes / (1024.0 * 1024.0)
    }

    companion object {
        /**
         * Creates a new ABRecording with auto-generated paths in app's private storage
         *
         * @param sessionId Unique session identifier (UUID recommended)
         * @param baseDir Base directory for recordings (app private storage)
         * @param durationSeconds Expected recording duration
         * @param componentStates Snapshot of current component states
         */
        fun create(
            sessionId: String,
            baseDir: File,
            durationSeconds: Int,
            componentStates: ComponentState
        ): ABRecording {
            val timestamp = Instant.now()
            val recordingDir = File(baseDir, "recording_$sessionId")
            recordingDir.mkdirs()

            return ABRecording(
                sessionId = sessionId,
                timestamp = timestamp,
                durationSeconds = durationSeconds,
                rawFilePath = File(recordingDir, "raw.wav").absolutePath,
                farEndFilePath = File(recordingDir, "far_end.wav").absolutePath,
                enhancedFilePath = File(recordingDir, "enhanced.wav").absolutePath,
                metadataFilePath = File(recordingDir, "metadata.json").absolutePath,
                componentStates = componentStates,
                displayName = "Recording_${timestamp.epochSecond}",
                totalSizeBytes = estimateSize(durationSeconds)
            )
        }

        /**
         * Estimates storage size for a recording of given duration
         *
         * Formula: 3 files × 48 kHz × 16-bit × duration + metadata overhead
         * = 3 × 96000 bytes/sec × duration + 10 KB
         */
        private fun estimateSize(durationSeconds: Int): Long {
            val bytesPerSecond = 96000L // 48 kHz × 2 bytes × 1 channel
            val filesCount = 3
            val metadataOverhead = 10240L // 10 KB
            return filesCount * bytesPerSecond * durationSeconds + metadataOverhead
        }
    }
}
