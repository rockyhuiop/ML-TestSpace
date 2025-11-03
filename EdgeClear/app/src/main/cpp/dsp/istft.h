#pragma once

#include "cola_windows.h"
#include "pffft.h"
#include <cstring>
#include <algorithm>

namespace edgeclear {
namespace dsp {

/**
 * ISTFT: Inverse Short-Time Fourier Transform (Synthesis)
 *
 * Converts frequency domain back to time domain using:
 * - 320-point inverse FFT
 * - sqrt-Hann window
 * - Overlap-add reconstruction
 * - 50% overlap (10 ms hop = 160 samples)
 *
 * Constitution requirements:
 * - COLA perfect reconstruction <0.5 LSB
 * - Soft limiting to prevent overflow
 * - TPDF dither on output (handled in DSPFrameBuffer)
 * - 10 ms hop matches DSPFrameBuffer::kFrameSize = 160
 */
class ISTFT {
public:
    static constexpr size_t kFFTSize = 320;
    static constexpr size_t kHopSize = 160;  // Matches DSPFrameBuffer
    static constexpr size_t kNumBins = kFFTSize / 2 + 1;  // 161

    ISTFT() : fft_setup_(nullptr) {
        fft_setup_ = pffft_new_setup(kFFTSize, PFFFT_REAL);

        std::memset(output_buffer_, 0, sizeof(output_buffer_));
        std::memset(window_, 0, sizeof(window_));
        std::memset(work_buffer_, 0, sizeof(work_buffer_));

        // Generate sqrt-Hann window (same as STFT)
        COLAWindows::GenerateSqrtHann(window_);
    }

    ~ISTFT() {
        if (fft_setup_) {
            pffft_destroy_setup(fft_setup_);
        }
    }

    // Non-copyable
    ISTFT(const ISTFT&) = delete;
    ISTFT& operator=(const ISTFT&) = delete;

    /**
     * Process one hop of frequency-domain data.
     *
     * Input: frequency-domain complex bins (161 bins)
     * Output: time-domain samples (160 samples for this hop @ 16 kHz = 10 ms)
     *
     * Overlap-add reconstruction:
     * - Inverse FFT
     * - Apply window
     * - Add to overlap buffer (accumulate with previous frame)
     * - Extract first 160 samples as output
     * - Shift buffer left for next iteration
     *
     * @param input Complex spectrum (kNumBins = 161, 322 floats interleaved real/imag from pffft)
     * @param output Time-domain samples (kHopSize = 160 samples)
     */
    void ProcessHop(const float* input, float* output) {
        // Inverse FFT (no scaling needed - pffft handles 1/N internally for BACKWARD)
        float ifft_out[kFFTSize];
        pffft_transform_ordered(fft_setup_, input, ifft_out, work_buffer_,
                               PFFFT_BACKWARD);

        // Apply window
        float windowed[kFFTSize];
        for (size_t i = 0; i < kFFTSize; ++i) {
            windowed[i] = ifft_out[i] * window_[i];
        }

        // Overlap-add: accumulate into output buffer
        for (size_t i = 0; i < kFFTSize; ++i) {
            output_buffer_[i] += windowed[i];
        }

        // Apply soft limiting to prevent overflow
        // Clamp to ±2.0 before int16 conversion (provides headroom)
        for (size_t i = 0; i < kHopSize; ++i) {
            output_buffer_[i] = std::max(-2.0f, std::min(2.0f, output_buffer_[i]));
        }

        // Extract first kHopSize samples as output
        std::memcpy(output, output_buffer_, kHopSize * sizeof(float));

        // Shift buffer left (discard output samples, keep overlap)
        std::memmove(output_buffer_, output_buffer_ + kHopSize,
                    kHopSize * sizeof(float));

        // Zero the right half (will be filled by next overlap-add)
        std::memset(output_buffer_ + kHopSize, 0, kHopSize * sizeof(float));
    }

    /**
     * Reset state (e.g., on session restart or discontinuity).
     */
    void Reset() {
        std::memset(output_buffer_, 0, sizeof(output_buffer_));
    }

private:
    PFFFT_Setup* fft_setup_;
    float output_buffer_[kFFTSize];  // Overlap-add buffer
    float window_[kFFTSize];         // sqrt-Hann window LUT
    float work_buffer_[kFFTSize];    // pffft work buffer
};

}  // namespace dsp
}  // namespace edgeclear
