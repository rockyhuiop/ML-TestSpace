package com.edgeclear.audio

import java.time.Instant
import java.time.Duration
import java.util.UUID

/**
 * AudioSession: Represents a single monitoring session
 *
 * Lifecycle: Created on "Start Monitoring", destroyed on "Stop Monitoring"
 * See data-model.md for full entity specification.
 */
data class AudioSession(
    val sessionId: UUID = UUID.randomUUID(),
    val startTime: Instant = Instant.now(),
    var duration: Duration = Duration.ZERO,
    var xrunCount: Int = 0,
    var currentPreset: ProcessingPreset = ProcessingPreset.QUALITY,
    val componentState: ComponentState = ComponentState(),
    var isRecording: Boolean = false,
    var recordingStartTime: Instant? = null,
    var recordingDuration: Int = 15  // seconds
) {
    /**
     * Session state machine
     */
    enum class State {
        IDLE,
        MONITORING,
        MONITORING_AND_RECORDING
    }

    fun getState(): State {
        return when {
            !isRecording -> State.MONITORING
            else -> State.MONITORING_AND_RECORDING
        }
    }

    /**
     * Update duration (called every second by metrics poller)
     */
    fun updateDuration() {
        duration = Duration.between(startTime, Instant.now())
    }
}

/**
 * ProcessingPreset: Configuration bundle for DSP parameters
 */
enum class ProcessingPreset(
    val displayName: String,
    val targetLatencyMs: Int,
    val bufferHops: Int,
    val aecFilterLength: Int,
    val denoiserFrameRate: Int,
    val resEnabled: Boolean,
    val cameraFrameRateHz: Int
) {
    LOW_LATENCY(
        "Low-latency",
        targetLatencyMs = 25,
        bufferHops = 1,
        aecFilterLength = 4,
        denoiserFrameRate = 1,
        resEnabled = false,
        cameraFrameRateHz = 15
    ),
    QUALITY(
        "Quality",
        targetLatencyMs = 38,
        bufferHops = 2,
        aecFilterLength = 8,
        denoiserFrameRate = 1,
        resEnabled = true,
        cameraFrameRateHz = 30
    ),
    BATTERY_SAVER(
        "Battery saver",
        targetLatencyMs = 35,
        bufferHops = 2,
        aecFilterLength = 8,
        denoiserFrameRate = 2,
        resEnabled = false,
        cameraFrameRateHz = 10
    );

    companion object {
        fun fromString(name: String): ProcessingPreset {
            return values().find { it.displayName == name } ?: QUALITY
        }
    }
}
