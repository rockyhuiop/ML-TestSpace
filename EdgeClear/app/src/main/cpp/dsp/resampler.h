#pragma once

#include <cstddef>
#include <cstring>

namespace edgeclear {
namespace dsp {

/**
 * Resampler: Polyphase FIR resampler for 48→16 kHz and 16→48 kHz
 *
 * TODO: Implement proper polyphase FIR filters
 * Current implementation: Stub (simple decimation/interpolation)
 *
 * Production requirements:
 * - Low-pass filter to prevent aliasing
 * - Linear phase FIR
 * - <0.4 ms latency budget
 */
class Resampler {
public:
    /**
     * Downsample 48 kHz → 16 kHz (3:1 decimation)
     */
    static void Downsample_48_to_16(const float* input_48k, size_t num_samples_48k,
                                     float* output_16k) {
        // Simple decimation (take every 3rd sample)
        // TODO: Add low-pass anti-aliasing filter
        const size_t num_output = num_samples_48k / 3;
        for (size_t i = 0; i < num_output; ++i) {
            output_16k[i] = input_48k[i * 3];
        }
    }

    /**
     * Upsample 16 kHz → 48 kHz (1:3 interpolation)
     */
    static void Upsample_16_to_48(const float* input_16k, size_t num_samples_16k,
                                   float* output_48k) {
        // Simple zero-stuffing interpolation
        // TODO: Add low-pass reconstruction filter
        for (size_t i = 0; i < num_samples_16k; ++i) {
            output_48k[i * 3] = input_16k[i];
            output_48k[i * 3 + 1] = 0.0f;
            output_48k[i * 3 + 2] = 0.0f;
        }
    }
};

}  // namespace dsp
}  // namespace edgeclear
