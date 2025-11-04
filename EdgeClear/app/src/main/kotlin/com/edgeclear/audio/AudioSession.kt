package com.edgeclear.audio
// Phase 7: Updated to use new ProcessingPreset data class from ProcessingPreset.kt

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
    var currentPreset: ProcessingPreset = Presets.DEFAULT,
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
