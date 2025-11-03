#include "dtd.h"
#include <android/log.h>
#include <cmath>
#include <cstring>

#define LOG_TAG "EdgeClear-DTD"
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)

namespace edgeclear {
namespace aec {

DTD::DTD()
    : state_(SILENCE),
      hangover_counter_(0) {
    std::memset(near_power_, 0, sizeof(near_power_));
    std::memset(far_power_, 0, sizeof(far_power_));
    std::memset(error_power_, 0, sizeof(error_power_));
    std::memset(cross_real_, 0, sizeof(cross_real_));
    std::memset(cross_imag_, 0, sizeof(cross_imag_));
}

DTD::State DTD::ProcessFrame(const float* near_fft,
                              const float* far_fft,
                              const float* error_fft) {
    // Update power and cross-correlation estimates
    UpdatePowerEstimates(near_fft, far_fft, error_fft);

    // Compute average MSC across frequency bins
    float msc = ComputeMSC();

    // Compute total power in near and far signals
    float near_total = 0.0f;
    float far_total = 0.0f;
    for (int k = 0; k < kNumBins; ++k) {
        near_total += near_power_[k];
        far_total += far_power_[k];
    }

    // Determine new state
    State new_state = DetermineState(msc, near_total, far_total);

    // Apply hangover for stability
    if (new_state != state_) {
        hangover_counter_++;
        if (hangover_counter_ >= kHangoverFrames) {
            state_ = new_state;
            hangover_counter_ = 0;
        }
    } else {
        hangover_counter_ = 0;
    }

    return state_;
}

void DTD::Reset() {
    state_ = SILENCE;
    hangover_counter_ = 0;
    std::memset(near_power_, 0, sizeof(near_power_));
    std::memset(far_power_, 0, sizeof(far_power_));
    std::memset(error_power_, 0, sizeof(error_power_));
    std::memset(cross_real_, 0, sizeof(cross_real_));
    std::memset(cross_imag_, 0, sizeof(cross_imag_));
}

void DTD::UpdatePowerEstimates(const float* near_fft,
                                const float* far_fft,
                                const float* error_fft) {
    // EMA smoothing: P[k] = α·P[k] + (1-α)·|X[k]|²
    // Cross-correlation: Pxy[k] = α·Pxy[k] + (1-α)·X*[k]·Y[k]

    for (int k = 0; k < kNumBins; ++k) {
        // Extract complex values
        float near_re = near_fft[2 * k];
        float near_im = near_fft[2 * k + 1];
        float far_re = far_fft[2 * k];
        float far_im = far_fft[2 * k + 1];
        float error_re = error_fft[2 * k];
        float error_im = error_fft[2 * k + 1];

        // Update power estimates
        float near_mag_sq = near_re * near_re + near_im * near_im;
        float far_mag_sq = far_re * far_re + far_im * far_im;
        float error_mag_sq = error_re * error_re + error_im * error_im;

        near_power_[k] = kPowerSmoothingAlpha * near_power_[k] +
                         (1.0f - kPowerSmoothingAlpha) * near_mag_sq;
        far_power_[k] = kPowerSmoothingAlpha * far_power_[k] +
                        (1.0f - kPowerSmoothingAlpha) * far_mag_sq;
        error_power_[k] = kPowerSmoothingAlpha * error_power_[k] +
                          (1.0f - kPowerSmoothingAlpha) * error_mag_sq;

        // Update cross-correlation: Near*[k] · Far[k]
        // (near_re - j·near_im) · (far_re + j·far_im)
        // = (near_re·far_re + near_im·far_im) + j·(-near_im·far_re + near_re·far_im)
        float cross_re = near_re * far_re + near_im * far_im;
        float cross_im = -near_im * far_re + near_re * far_im;

        cross_real_[k] = kPowerSmoothingAlpha * cross_real_[k] +
                         (1.0f - kPowerSmoothingAlpha) * cross_re;
        cross_imag_[k] = kPowerSmoothingAlpha * cross_imag_[k] +
                         (1.0f - kPowerSmoothingAlpha) * cross_im;
    }
}

float DTD::ComputeMSC() {
    // MSC = Σ |Pxy[k]|² / (Σ Pxx[k] · Σ Pyy[k])
    // Averaged across frequency bins

    float cross_sum = 0.0f;
    float near_sum = 0.0f;
    float far_sum = 0.0f;

    for (int k = 0; k < kNumBins; ++k) {
        float cross_mag_sq = cross_real_[k] * cross_real_[k] +
                             cross_imag_[k] * cross_imag_[k];
        cross_sum += cross_mag_sq;
        near_sum += near_power_[k];
        far_sum += far_power_[k];
    }

    // Avoid division by zero
    if (near_sum < 1e-10f || far_sum < 1e-10f) {
        return 0.0f;
    }

    return cross_sum / (near_sum * far_sum);
}

DTD::State DTD::DetermineState(float msc, float near_power, float far_power) {
    // Check for silence
    if (near_power < kSilenceThreshold && far_power < kSilenceThreshold) {
        return SILENCE;
    }

    // Check for far-end only (high coherence)
    if (msc > kMSCHighThreshold && far_power > kSilenceThreshold) {
        return FAR_END_ONLY;
    }

    // Check for near-end only (low coherence, near active)
    if (msc < kMSCLowThreshold && near_power > kSilenceThreshold &&
        far_power < kSilenceThreshold) {
        return NEAR_END_ONLY;
    }

    // Both active with low coherence → double-talk
    if (msc < kMSCLowThreshold && near_power > kSilenceThreshold &&
        far_power > kSilenceThreshold) {
        return DOUBLE_TALK;
    }

    // Default: maintain current state
    return state_;
}

}  // namespace aec
}  // namespace edgeclear
