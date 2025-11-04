package com.edgeclear.audio

/**
 * Processing preset configuration bundle defining DSP parameters.
 *
 * Each preset balances latency, quality, and CPU/battery usage through
 * configurable parameters for buffer sizing, AEC filter length, denoiser
 * frame rate, RES post-filter, and camera frame rate (for AV-VAD).
 *
 * Presets are immutable and loaded once at app initialization.
 *
 * @property name Preset identifier ("Low-latency", "Quality", "Battery saver")
 * @property targetLatencyMs Target end-to-end latency in milliseconds (20-40 ms range)
 * @property bufferHops Number of hops to buffer in SPSC queues (1 = tight, 6 = safe)
 * @property aecFilterLength Number of AEC partitions (4 = 85 ms, 8 = 170 ms, 16 = 340 ms)
 * @property denoiserFrameRate Process every Nth hop (1 = every hop, 2 = every other hop)
 * @property resEnabled Enable/disable RES post-filter
 * @property cameraFrameRateHz Camera frame rate for AV-VAD in Hz (10-30 range)
 */
data class ProcessingPreset(
    val name: String,
    val targetLatencyMs: Int,
    val bufferHops: Int,
    val aecFilterLength: Int,
    val denoiserFrameRate: Int,
    val resEnabled: Boolean,
    val cameraFrameRateHz: Int
) {
    init {
        require(name.isNotBlank()) { "Preset name cannot be blank" }
        require(targetLatencyMs in 20..40) {
            "Target latency must be 20-40 ms, got $targetLatencyMs"
        }
        require(bufferHops in 1..6) {
            "Buffer hops must be 1-6, got $bufferHops"
        }
        require(aecFilterLength in setOf(4, 8, 16)) {
            "AEC filter length must be 4, 8, or 16 partitions, got $aecFilterLength"
        }
        require(denoiserFrameRate in setOf(1, 2)) {
            "Denoiser frame rate must be 1 or 2, got $denoiserFrameRate"
        }
        require(cameraFrameRateHz in 10..30) {
            "Camera frame rate must be 10-30 Hz, got $cameraFrameRateHz"
        }
    }

    /**
     * Estimated echo tail length in milliseconds based on AEC filter length.
     * Formula: (partitions * 1024 samples) / 16000 Hz * 1000 ms/s
     */
    val echoTailMs: Int
        get() = (aecFilterLength * 1024 * 1000) / 16000

    /**
     * Minimum achievable latency based on buffer hops and DSP budget.
     * Formula: input_buffer (10 ms) + bufferHops * 10 ms + DSP (6 ms) + output_buffer (10 ms)
     */
    val minimumLatencyMs: Int
        get() = 10 + (bufferHops * 10) + 6 + 10
}
