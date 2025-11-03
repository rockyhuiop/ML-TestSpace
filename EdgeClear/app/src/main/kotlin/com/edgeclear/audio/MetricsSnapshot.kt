package com.edgeclear.audio

import java.time.Instant

/**
 * MetricsSnapshot: Real-time DSP metrics at a given timestamp
 *
 * Created every 1 second while monitoring active.
 * Stored in circular buffer (last 60 snapshots = 1 minute history).
 *
 * See data-model.md for validation rules and field specifications.
 */
data class MetricsSnapshot(
    val timestamp: Instant = Instant.now(),
    val erle_dB: Float,          // 0.0-30.0 dB
    val siSdr_dB: Float,         // -10.0 to +20.0 dB
    val cpuMs: Float,            // 0.0-10.0 ms per 10 ms hop
    val latencyMs: Float,        // 0.0-100.0 ms
    val xrunCount: Int,          // Cumulative since session start
    val vadState: VadState,
    val dtdState: DtdState,
    val avVadMode: String        // "active" | "fallback" | "disabled"
) {
    /**
     * Check if CPU budget violated (>6 ms per hop)
     */
    fun isCpuBudgetViolated(): Boolean = cpuMs > 6.0f

    /**
     * Check if latency budget violated (>40 ms)
     */
    fun isLatencyBudgetViolated(): Boolean = latencyMs > 40.0f

    companion object {
        /**
         * Create from native metrics
         */
        fun fromNative(native: com.edgeclear.jni.NativeMetrics): MetricsSnapshot {
            return MetricsSnapshot(
                erle_dB = native.erle_dB.coerceIn(0.0f, 30.0f),
                siSdr_dB = native.siSdr_dB.coerceIn(-10.0f, 20.0f),
                cpuMs = native.cpuMs.coerceIn(0.0f, 10.0f),
                latencyMs = native.latencyMs.coerceIn(0.0f, 100.0f),
                xrunCount = native.xrunCount,
                vadState = VadState.fromInt(native.vadState),
                dtdState = DtdState.fromInt(native.dtdState),
                avVadMode = native.avVadMode
            )
        }
    }
}

/**
 * Voice Activity Detection state
 */
enum class VadState {
    INACTIVE,
    ACTIVE;

    companion object {
        fun fromInt(value: Int): VadState = if (value == 0) INACTIVE else ACTIVE
    }
}

/**
 * Double-Talk Detection state
 */
enum class DtdState {
    SILENCE,
    NEAR_END_ONLY,
    FAR_END_ONLY,
    DOUBLE_TALK;

    companion object {
        fun fromInt(value: Int): DtdState {
            return when (value) {
                0 -> SILENCE
                1 -> NEAR_END_ONLY
                2 -> FAR_END_ONLY
                3 -> DOUBLE_TALK
                else -> NEAR_END_ONLY
            }
        }
    }
}
