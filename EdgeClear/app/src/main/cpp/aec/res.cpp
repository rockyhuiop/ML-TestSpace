#include "res.h"
#include <android/log.h>
#include <cmath>
#include <cstring>
#include <algorithm>

#define LOG_TAG "EdgeClear-RES"
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)

namespace edgeclear {
namespace aec {

RES::RES() {
    std::memset(gain_, 0, sizeof(gain_));
    std::memset(error_power_, 0, sizeof(error_power_));
    std::memset(far_power_, 0, sizeof(far_power_));

    // Initialize gains to unity (no suppression)
    for (int k = 0; k < kNumBins; ++k) {
        gain_[k] = 1.0f;
    }
}

void RES::ProcessFrame(float* error_fft,
                       const float* far_fft,
                       bool is_double_talk) {
    // Update power estimates
    UpdatePowerEstimates(error_fft, far_fft);

    // Compute suppression gain
    ComputeGain(is_double_talk);

    // Apply gain to error spectrum
    ApplyGain(error_fft);
}

void RES::Reset() {
    std::memset(error_power_, 0, sizeof(error_power_));
    std::memset(far_power_, 0, sizeof(far_power_));

    // Reset gains to unity
    for (int k = 0; k < kNumBins; ++k) {
        gain_[k] = 1.0f;
    }
}

void RES::UpdatePowerEstimates(const float* error_fft, const float* far_fft) {
    // EMA smoothing: P[k] = α·P[k] + (1-α)·|X[k]|²
    for (int k = 0; k < kNumBins; ++k) {
        float error_re = error_fft[2 * k];
        float error_im = error_fft[2 * k + 1];
        float far_re = far_fft[2 * k];
        float far_im = far_fft[2 * k + 1];

        float error_mag_sq = error_re * error_re + error_im * error_im;
        float far_mag_sq = far_re * far_re + far_im * far_im;

        error_power_[k] = kPowerSmoothingAlpha * error_power_[k] +
                          (1.0f - kPowerSmoothingAlpha) * error_mag_sq;
        far_power_[k] = kPowerSmoothingAlpha * far_power_[k] +
                        (1.0f - kPowerSmoothingAlpha) * far_mag_sq;
    }
}

void RES::ComputeGain(bool is_double_talk) {
    // Wiener-like gain: G[k] = max(1 - λ·P_far[k]/P_error[k], G_min)
    // During double-talk: reduce suppression (use smaller λ)

    float effective_suppression = is_double_talk ?
        kSuppressionFactor * 0.5f : kSuppressionFactor;

    for (int k = 0; k < kNumBins; ++k) {
        float ratio = 0.0f;
        if (error_power_[k] > kMinPower) {
            ratio = far_power_[k] / error_power_[k];
        }

        // Compute target gain
        float target_gain = 1.0f - effective_suppression * ratio;
        target_gain = std::max(target_gain, kGainFloor);
        target_gain = std::min(target_gain, 1.0f);

        // Smooth gain transitions
        gain_[k] = kGainSmoothingAlpha * gain_[k] +
                   (1.0f - kGainSmoothingAlpha) * target_gain;
    }
}

void RES::ApplyGain(float* error_fft) {
    // Apply gain to complex spectrum
    for (int k = 0; k < kNumBins; ++k) {
        error_fft[2 * k] *= gain_[k];      // Real part
        error_fft[2 * k + 1] *= gain_[k];  // Imaginary part
    }
}

}  // namespace aec
}  // namespace edgeclear
