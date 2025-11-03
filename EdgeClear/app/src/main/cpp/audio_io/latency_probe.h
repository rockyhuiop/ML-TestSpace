#pragma once

namespace edgeclear {
namespace audio_io {

/**
 * LatencyProbe: Loopback latency measurement tool
 *
 * Measures end-to-end latency via impulse response correlation.
 *
 * TODO: Implement full impulse loopback measurement (research.md R4)
 * Current implementation: Returns estimated latency based on buffer configuration
 */
class LatencyProbe {
public:
    /**
     * Measure latency (stub implementation).
     *
     * @param sampleRate Sample rate in Hz
     * @param bufferHops Number of buffer hops
     * @return Estimated latency in milliseconds
     */
    static float MeasureLatency(int32_t sampleRate, int32_t bufferHops) {
        // Stub: Estimate latency from buffer configuration
        // Formula: (input buffer + output buffer + processing) latency
        // Input/output: 1 hop each = 10 ms * 2 = 20 ms
        // Ring buffering: bufferHops * 10 ms
        // Processing: ~4 ms (STFT/ISTFT overhead)

        const float hopDuration = 10.0f;  // 10 ms @ 16 kHz
        const float ioLatency = hopDuration * 2;  // Input + output
        const float bufferLatency = hopDuration * bufferHops;
        const float processingLatency = 4.0f;  // DSP overhead

        return ioLatency + bufferLatency + processingLatency;
    }
};

}  // namespace audio_io
}  // namespace edgeclear
