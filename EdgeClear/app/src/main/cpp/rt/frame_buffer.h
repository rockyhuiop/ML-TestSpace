#pragma once

#include <cstdint>
#include <cstring>

namespace edgeclear {
namespace rt {

/**
 * DSPFrameBuffer: Single audio frame (160 samples @ 16 kHz, one 10 ms hop)
 *
 * Used for passing audio between RT callback and DSP worker thread via SPSC queue.
 *
 * Constitution requirements:
 * - 10 ms hop fixed (160 samples @ 16 kHz)
 * - int16 I/O with 3 dB headroom (clamp to ±16383)
 * - Timestamp for alignment validation
 */
struct DSPFrameBuffer {
    static constexpr int kFrameSize = 160;  // 10 ms @ 16 kHz
    static constexpr int16_t kMaxSample = 16383;  // 3 dB headroom
    static constexpr int16_t kMinSample = -16383;

    int16_t samples[kFrameSize];  // Audio samples
    uint64_t timestamp;           // Sample count since session start
    bool is_clipping;             // True if any sample clipped

    DSPFrameBuffer() : timestamp(0), is_clipping(false) {
        std::memset(samples, 0, sizeof(samples));
    }

    /**
     * Copy samples with clipping detection and 3 dB headroom enforcement.
     */
    void CopySamples(const int16_t* src, int num_samples) {
        is_clipping = false;
        const int count = (num_samples < kFrameSize) ? num_samples : kFrameSize;

        for (int i = 0; i < count; ++i) {
            // Clamp to ±16383 (3 dB headroom reserved)
            if (src[i] > kMaxSample) {
                samples[i] = kMaxSample;
                is_clipping = true;
            } else if (src[i] < kMinSample) {
                samples[i] = kMinSample;
                is_clipping = true;
            } else {
                samples[i] = src[i];
            }
        }

        // Zero-pad if fewer samples provided
        if (count < kFrameSize) {
            std::memset(samples + count, 0, (kFrameSize - count) * sizeof(int16_t));
        }
    }

    /**
     * Convert int16 to float32 for DSP processing.
     * Scale: int16 [-16383, 16383] -> float [-1.0, 1.0]
     */
    void ToFloat(float* dest) const {
        constexpr float scale = 1.0f / 16383.0f;
        for (int i = 0; i < kFrameSize; ++i) {
            dest[i] = samples[i] * scale;
        }
    }

    /**
     * Convert float32 to int16 with dithering.
     * Includes TPDF dither (±1 LSB) to reduce quantization noise.
     */
    void FromFloat(const float* src) {
        constexpr float scale = 16383.0f;
        is_clipping = false;

        for (int i = 0; i < kFrameSize; ++i) {
            // TPDF dither: triangular probability density function
            // Generate two uniform random values, sum gives triangular distribution
            const float dither = ((rand() & 0xFFFF) / 32768.0f - 1.0f) +
                                ((rand() & 0xFFFF) / 32768.0f - 1.0f);

            float sample = src[i] * scale + dither;

            // Clamp to ±16383
            if (sample > kMaxSample) {
                samples[i] = kMaxSample;
                is_clipping = true;
            } else if (sample < kMinSample) {
                samples[i] = kMinSample;
                is_clipping = true;
            } else {
                samples[i] = static_cast<int16_t>(sample);
            }
        }
    }
};

}  // namespace rt
}  // namespace edgeclear
