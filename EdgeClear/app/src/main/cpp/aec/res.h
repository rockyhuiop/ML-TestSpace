#pragma once

#include <cstdint>

namespace edgeclear {
namespace aec {

/**
 * RES: Residual Echo Suppressor
 *
 * Applies frequency-domain gain to suppress residual echo after AEC.
 *
 * Algorithm:
 * - Computes Wiener-like gain: G[k] = max(1 - λ·P_far[k]/P_error[k], G_min)
 * - Safety floor: G_min = -18 dB (prevents over-suppression)
 * - Smooth gain transitions to avoid artifacts
 * - Only active when far-end is present
 *
 * Constitution compliance:
 * - RT-safe: No allocations
 * - Low CPU: Simple per-bin gain calculation
 * - Graceful: Safety floor preserves near-end speech
 *
 * References:
 * - Gustafsson et al. (1998): "Spectral Subtraction using Reduced Delay Convolution and Adaptive Averaging"
 * - ITU-T G.168: Residual echo suppression recommendations
 */
class RES {
public:
    static constexpr int kNumBins = 161;  // 320-point FFT / 2 + 1

    RES();
    ~RES() = default;

    /**
     * Process one frame.
     *
     * @param error_fft AEC error spectrum (complex, 161 bins) - modified in place
     * @param far_fft Far-end spectrum (complex, 161 bins)
     * @param is_double_talk true if DTD detects double-talk
     */
    void ProcessFrame(float* error_fft,
                      const float* far_fft,
                      bool is_double_talk);

    /**
     * Reset suppressor state.
     */
    void Reset();

private:
    // Gain state (for smoothing)
    float gain_[kNumBins];

    // Power estimates
    float error_power_[kNumBins];
    float far_power_[kNumBins];

    // Configuration
    static constexpr float kSuppressionFactor = 2.0f;  // λ parameter
    static constexpr float kGainFloor = 0.125f;  // -18 dB
    static constexpr float kGainSmoothingAlpha = 0.7f;  // Gain smoothing
    static constexpr float kPowerSmoothingAlpha = 0.9f;  // Power smoothing
    static constexpr float kMinPower = 1e-10f;

    // Internal methods
    void UpdatePowerEstimates(const float* error_fft, const float* far_fft);
    void ComputeGain(bool is_double_talk);
    void ApplyGain(float* error_fft);
};

}  // namespace aec
}  // namespace edgeclear
