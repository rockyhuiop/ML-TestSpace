package com.edgeclear.audio

/**
 * Static preset definitions for EdgeClear audio processing.
 *
 * Three presets optimize for different use cases:
 * - LOW_LATENCY: Minimal latency (~25 ms) for real-time scenarios
 * - QUALITY: Balanced quality and latency (~38 ms) for general use
 * - BATTERY_SAVER: Reduced CPU usage (~35 ms) for extended battery life
 *
 * Preset parameters from plan.md data-model section:
 * | Preset          | targetLatencyMs | bufferHops | aecFilterLength | denoiserFrameRate | resEnabled | cameraFrameRateHz |
 * |-----------------|-----------------|------------|-----------------|-------------------|------------|-------------------|
 * | Low-latency     | 25              | 1          | 4               | 1                 | false      | 15                |
 * | Quality         | 38              | 2          | 8               | 1                 | true       | 30                |
 * | Battery saver   | 35              | 2          | 8               | 2                 | false      | 10                |
 */
object Presets {
    /**
     * Low-latency preset: Optimized for minimal end-to-end latency.
     *
     * Trade-offs:
     * - Tight buffering (1 hop) reduces latency but increases XRun risk
     * - Shorter AEC filter (4 partitions = 85 ms) may miss long echoes
     * - RES disabled to save 0.2 ms CPU
     * - Lower camera frame rate (15 Hz) reduces AV-VAD CPU overhead
     *
     * Target latency: ~25 ms
     * Expected ERLE: 9-12 dB (shorter AEC filter)
     */
    val LOW_LATENCY = ProcessingPreset(
        name = "Low-latency",
        targetLatencyMs = 25,
        bufferHops = 1,
        aecFilterLength = 4,
        denoiserFrameRate = 1,
        resEnabled = false,
        cameraFrameRateHz = 15
    )

    /**
     * Quality preset: Balanced quality and latency for general use.
     *
     * Trade-offs:
     * - Moderate buffering (2 hops) for jitter absorption
     * - Full AEC filter (8 partitions = 170 ms) handles music far-end
     * - RES enabled for +3 dB ERLE improvement
     * - Full camera frame rate (30 Hz) for responsive AV-VAD
     *
     * Target latency: ~38 ms
     * Expected ERLE: 15-18 dB (full pipeline)
     */
    val QUALITY = ProcessingPreset(
        name = "Quality",
        targetLatencyMs = 38,
        bufferHops = 2,
        aecFilterLength = 8,
        denoiserFrameRate = 1,
        resEnabled = true,
        cameraFrameRateHz = 30
    )

    /**
     * Battery saver preset: Reduced CPU usage for extended battery life.
     *
     * Trade-offs:
     * - Same buffering as Quality (2 hops)
     * - Full AEC filter (8 partitions) for ERLE
     * - RES disabled to save 0.2 ms CPU
     * - Denoiser every other hop (saves ~1.5 ms CPU)
     * - Lowest camera frame rate (10 Hz) reduces AV-VAD overhead
     *
     * Target latency: ~35 ms
     * Expected CPU reduction: ~1.7 ms per hop (vs Quality)
     */
    val BATTERY_SAVER = ProcessingPreset(
        name = "Battery saver",
        targetLatencyMs = 35,
        bufferHops = 2,
        aecFilterLength = 8,
        denoiserFrameRate = 2,
        resEnabled = false,
        cameraFrameRateHz = 10
    )

    /**
     * Default preset on app launch.
     */
    val DEFAULT = QUALITY

    /**
     * All available presets in display order.
     */
    val ALL = listOf(LOW_LATENCY, QUALITY, BATTERY_SAVER)

    /**
     * Get preset by name (case-insensitive).
     *
     * @param name Preset name to look up
     * @return Matching preset, or null if not found
     */
    fun getByName(name: String): ProcessingPreset? {
        return ALL.find { it.name.equals(name, ignoreCase = true) }
    }
}
