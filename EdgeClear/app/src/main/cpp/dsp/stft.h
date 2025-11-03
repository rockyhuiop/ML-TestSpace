#pragma once

#include "cola_windows.h"
#include "pffft.h"
#include <cstring>

namespace edgeclear {
namespace dsp {

/**
 * STFT: Short-Time Fourier Transform (Analysis)
 *
 * Converts time-domain signal to frequency domain using:
 * - 320-point FFT (20 ms window @ 16 kHz)
 * - sqrt-Hann window
 * - 50% overlap (10 ms hop = 160 samples)
 *
 * Constitution requirements:
 * - FFT scaling: 1/N on forward transform
 * - COLA validated <0.5 LSB
 * - 10 ms hop matches DSPFrameBuffer::kFrameSize = 160
 *
 * Research (research.md R1, R5):
 * - pffft: ~26 μs per 320-pt FFT on SD 778G (scaled from 512-pt)
 * - Target: <0.8 ms for STFT analysis
 */
class STFT {
public:
    static constexpr size_t kFFTSize = 320;
    static constexpr size_t kHopSize = 160;  // 50% overlap, matches DSPFrameBuffer
    static constexpr size_t kNumBins = kFFTSize / 2 + 1;  // DC + positive frequencies = 161

    STFT() : fft_setup_(nullptr), window_initialized_(false) {
        // Initialize FFT (complex transform for real signal via pffft REAL mode)
        fft_setup_ = pffft_new_setup(kFFTSize, PFFFT_REAL);

        // Pre-allocate buffers
        std::memset(input_buffer_, 0, sizeof(input_buffer_));
        std::memset(window_, 0, sizeof(window_));
        std::memset(work_buffer_, 0, sizeof(work_buffer_));

        // Generate sqrt-Hann window
        COLAWindows::GenerateSqrtHann(window_);
        window_initialized_ = true;

        // Validate COLA property
        cola_error_ = COLAWindows::ValidateCOLA(window_);
    }

    ~STFT() {
        if (fft_setup_) {
            pffft_destroy_setup(fft_setup_);
        }
    }

    // Non-copyable
    STFT(const STFT&) = delete;
    STFT& operator=(const STFT&) = delete;

    /**
     * Process one hop of audio samples.
     *
     * Input: time-domain samples (160 new samples @ 16 kHz = 10 ms)
     * Output: frequency-domain complex bins (161 bins: DC + 1..160)
     *
     * The input_buffer maintains 320 samples with 50% overlap:
     * - Shift left by 160 (discard old half)
     * - Copy new 160 samples to right half
     * - Apply window
     * - FFT
     *
     * @param input New samples (kHopSize = 160 samples)
     * @param output Complex spectrum (kNumBins = 161 complex pairs, 322 floats interleaved real/imag)
     */
    void ProcessHop(const float* input, float* output) {
        // Shift buffer left (discard old 256 samples)
        std::memmove(input_buffer_, input_buffer_ + kHopSize,
                    kHopSize * sizeof(float));

        // Copy new samples to right half
        std::memcpy(input_buffer_ + kHopSize, input, kHopSize * sizeof(float));

        // Apply window
        float windowed[kFFTSize];
        for (size_t i = 0; i < kFFTSize; ++i) {
            windowed[i] = input_buffer_[i] * window_[i];
        }

        // FFT (forward transform with 1/N scaling)
        pffft_transform_ordered(fft_setup_, windowed, output, work_buffer_,
                               PFFFT_FORWARD);

        // Apply 1/N scaling (Constitution requirement)
        constexpr float scale = 1.0f / kFFTSize;
        for (size_t i = 0; i < kFFTSize; ++i) {
            output[i] *= scale;
        }
    }

    float GetCOLAError() const { return cola_error_; }
    bool IsWindowInitialized() const { return window_initialized_; }

private:
    PFFFT_Setup* fft_setup_;
    float input_buffer_[kFFTSize];  // Overlapping input buffer
    float window_[kFFTSize];        // sqrt-Hann window LUT
    float work_buffer_[kFFTSize];   // pffft work buffer
    float cola_error_;
    bool window_initialized_;
};

}  // namespace dsp
}  // namespace edgeclear
