package com.edgeclear.audio

import android.content.Context
import android.os.Environment
import android.os.StatFs
import java.io.File

/**
 * Manages storage availability checks for A/B recordings.
 *
 * Estimates required storage for recordings and validates sufficient space
 * before starting recording sessions.
 *
 * Storage estimates (per second @ 48 kHz, 16-bit PCM):
 * - 3 files (raw, far-end, enhanced): 3 × 96 KB/sec = 288 KB/sec
 * - 30 seconds: ~8.6 MB + metadata overhead (~10 KB) ≈ 9 MB
 * - 15 seconds: ~4.3 MB + metadata overhead ≈ 5 MB
 */
class StorageManager(private val context: Context) {

    /**
     * Returns the recordings directory (app-private storage)
     */
    fun getRecordingsDirectory(): File {
        val recordingsDir = File(context.filesDir, "recordings")
        if (!recordingsDir.exists()) {
            recordingsDir.mkdirs()
        }
        return recordingsDir
    }

    /**
     * Checks if sufficient storage is available for a recording of given duration
     *
     * @param durationSeconds Recording duration in seconds
     * @return Pair<Boolean, String> - (isAvailable, errorMessage)
     *         If isAvailable is true, errorMessage is empty
     *         If false, errorMessage contains human-readable error
     */
    fun checkStorageAvailability(durationSeconds: Int): Pair<Boolean, String> {
        val requiredBytes = estimateRequiredBytes(durationSeconds)
        val availableBytes = getAvailableStorageBytes()

        // Add 20% safety margin
        val requiredWithMargin = (requiredBytes * 1.2).toLong()

        return if (availableBytes >= requiredWithMargin) {
            Pair(true, "")
        } else {
            val requiredMB = requiredWithMargin / (1024.0 * 1024.0)
            val availableMB = availableBytes / (1024.0 * 1024.0)
            val errorMsg = String.format(
                "Insufficient storage: Need %.1f MB, only %.1f MB available",
                requiredMB,
                availableMB
            )
            Pair(false, errorMsg)
        }
    }

    /**
     * Estimates required storage in bytes for a recording
     *
     * Formula: 3 files × 48 kHz × 2 bytes × duration + metadata (10 KB)
     */
    private fun estimateRequiredBytes(durationSeconds: Int): Long {
        val sampleRate = 48000
        val bytesPerSample = 2 // 16-bit PCM
        val channels = 1 // Mono
        val filesCount = 3 // raw, far-end, enhanced
        val metadataOverhead = 10240L // 10 KB for JSON metadata

        val bytesPerSecond = sampleRate * bytesPerSample * channels
        return filesCount * bytesPerSecond * durationSeconds + metadataOverhead
    }

    /**
     * Returns available storage bytes on the device
     */
    private fun getAvailableStorageBytes(): Long {
        val path = context.filesDir
        val stat = StatFs(path.absolutePath)
        return stat.availableBlocksLong * stat.blockSizeLong
    }

    /**
     * Returns total storage bytes on the device
     */
    fun getTotalStorageBytes(): Long {
        val path = context.filesDir
        val stat = StatFs(path.absolutePath)
        return stat.blockCountLong * stat.blockSizeLong
    }

    /**
     * Returns human-readable storage info
     */
    fun getStorageInfo(): String {
        val available = getAvailableStorageBytes() / (1024.0 * 1024.0)
        val total = getTotalStorageBytes() / (1024.0 * 1024.0)
        val used = total - available
        val usedPercent = (used / total) * 100.0

        return String.format(
            "Storage: %.1f MB / %.1f MB (%.0f%% used)",
            available, total, usedPercent
        )
    }

    /**
     * Lists all recordings in the recordings directory
     */
    fun listRecordings(): List<File> {
        val recordingsDir = getRecordingsDirectory()
        return recordingsDir.listFiles()?.filter { it.isDirectory } ?: emptyList()
    }

    /**
     * Deletes old recordings to free up space (oldest first)
     *
     * @param targetFreeBytes Target free bytes to achieve
     * @return Number of recordings deleted
     */
    fun cleanupOldRecordings(targetFreeBytes: Long): Int {
        var deletedCount = 0
        val recordings = listRecordings().sortedBy { it.lastModified() }

        for (recording in recordings) {
            if (getAvailableStorageBytes() >= targetFreeBytes) {
                break
            }

            if (recording.deleteRecursively()) {
                deletedCount++
            }
        }

        return deletedCount
    }

    /**
     * Returns size of recordings directory in bytes
     */
    fun getRecordingsDirectorySize(): Long {
        val recordingsDir = getRecordingsDirectory()
        return recordingsDir.walkTopDown()
            .filter { it.isFile }
            .map { it.length() }
            .sum()
    }

    /**
     * Returns human-readable size of recordings directory
     */
    fun getRecordingsDirectorySizeStr(): String {
        val sizeBytes = getRecordingsDirectorySize()
        val sizeMB = sizeBytes / (1024.0 * 1024.0)
        return String.format("%.1f MB", sizeMB)
    }

    companion object {
        // Storage constants
        const val MIN_FREE_STORAGE_MB = 50L // Minimum free storage to maintain
        const val WARNING_THRESHOLD_MB = 100L // Warn user if below this threshold
    }
}
