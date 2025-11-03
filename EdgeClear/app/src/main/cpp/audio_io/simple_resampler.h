#pragma once

#include <cstdint>
#include <cstring>

namespace edgeclear {
namespace audio_io {

/**
 * SimpleResampler: 3:1 decimator/interpolator for 48↔16 kHz
 *
 * This is a lightweight implementation for RT callback use.
 * Uses simple linear interpolation/decimation with accumulation buffering.
 *
 * TODO (Post-M2): Replace with proper polyphase FIR filter for better quality
 * Current implementation trades quality for RT safety and low CPU.
 *
 * Constitution compliance:
 * - Zero allocations after construction
 * - No mutexes
 * - Predictable CPU (<0.5 ms per callback)
 */
class SimpleResampler {
public:
    static constexpr int kDecimationFactor = 3;  // 48 kHz → 16 kHz
    static constexpr int kInputFramesPerHop = 480;   // 10 ms @ 48 kHz
    static constexpr int kOutputFramesPerHop = 160;  // 10 ms @ 16 kHz

    SimpleResampler()
        : decim_buffer_size_(0),
          interp_buffer_size_(0) {
        std::memset(decim_buffer_, 0, sizeof(decim_buffer_));
        std::memset(interp_buffer_, 0, sizeof(interp_buffer_));
    }

    ~SimpleResampler() = default;

    /**
     * Decimate 48→16 kHz (accumulate and decimate).
     *
     * Accumulates input samples until 480 @ 48 kHz are available,
     * then decimates to 160 @ 16 kHz.
     *
     * @param input Input samples @ 48 kHz
     * @param input_count Number of input samples
     * @param output Output buffer @ 16 kHz (must hold 160 samples)
     * @param output_count Number of output samples written
     * @return true if a full hop (160 samples) was produced
     */
    bool Decimate(const int16_t* input, int32_t input_count,
                  int16_t* output, int32_t* output_count) {
        *output_count = 0;

        // Accumulate input into decimation buffer
        int32_t remaining = input_count;
        int32_t input_offset = 0;

        while (remaining > 0) {
            int32_t space = kInputFramesPerHop - decim_buffer_size_;
            int32_t to_copy = (remaining < space) ? remaining : space;

            std::memcpy(decim_buffer_ + decim_buffer_size_,
                       input + input_offset,
                       to_copy * sizeof(int16_t));

            decim_buffer_size_ += to_copy;
            input_offset += to_copy;
            remaining -= to_copy;

            // Check if we have enough for one hop
            if (decim_buffer_size_ >= kInputFramesPerHop) {
                // Decimate: take every 3rd sample
                for (int32_t i = 0; i < kOutputFramesPerHop; ++i) {
                    output[i] = decim_buffer_[i * kDecimationFactor];
                }

                *output_count = kOutputFramesPerHop;
                decim_buffer_size_ = 0;  // Reset buffer
                return true;
            }
        }

        return false;
    }

    /**
     * Interpolate 16→48 kHz (buffer and interpolate).
     *
     * Buffers 160 @ 16 kHz input, then interpolates to 480 @ 48 kHz.
     *
     * @param input Input samples @ 16 kHz (160 samples)
     * @param output Output buffer @ 48 kHz (must hold 480 samples)
     * @param output_count Number of output samples produced
     */
    void Interpolate(const int16_t* input, int16_t* output, int32_t* output_count) {
        // 16 kHz → 48 kHz: 160 input samples → 480 output samples
        for (int32_t i = 0; i < kInputFramesPerHop; ++i) {
            int32_t in_idx = i / kDecimationFactor;
            int32_t phase = i % kDecimationFactor;

            if (phase == 0) {
                output[i] = input[in_idx];
            } else {
                int32_t next_idx = in_idx + 1;
                if (next_idx < kOutputFramesPerHop) {
                    int32_t a = input[in_idx];
                    int32_t b = input[next_idx];
                    output[i] = static_cast<int16_t>(
                        a + ((b - a) * phase) / kDecimationFactor);
                } else {
                    output[i] = input[in_idx];
                }
            }
        }

        *output_count = kInputFramesPerHop;
    }

    /**
     * Reset state (e.g., on stream restart).
     */
    void Reset() {
        decim_buffer_size_ = 0;
        interp_buffer_size_ = 0;
        std::memset(decim_buffer_, 0, sizeof(decim_buffer_));
        std::memset(interp_buffer_, 0, sizeof(interp_buffer_));
    }

private:
    // Decimation accumulation buffer (48 kHz)
    int16_t decim_buffer_[kInputFramesPerHop];
    int32_t decim_buffer_size_;

    // Interpolation buffer (16 kHz) - for future use
    int16_t interp_buffer_[kOutputFramesPerHop];
    int32_t interp_buffer_size_;
};

}  // namespace audio_io
}  // namespace edgeclear
