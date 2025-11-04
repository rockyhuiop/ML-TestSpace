#include "aec.h"
#include <android/log.h>
#include <cmath>
#include <cstring>

#define LOG_TAG "EdgeClear-AEC"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace edgeclear {
namespace aec {

// Constants
static constexpr float kPowerSmoothingAlpha = 0.95f;  // EMA smoothing factor
static constexpr float kMinPower = 1e-10f;  // Numerical stability floor
static constexpr float kDoubleTalkStepScale = 0.1f;  // Reduce adaptation during DT

AEC::AEC(int32_t sampleRate, float stepSize)
    : sample_rate_(sampleRate),
      step_size_(stepSize),
      far_end_history_(nullptr),
      far_end_write_idx_(0),
      erle_db_(0.0f),
      fft_setup_(nullptr) {

    // Initialize filter coefficient pointers to null
    for (int p = 0; p < kNumPartitions; ++p) {
        filter_coeffs_[p] = nullptr;
    }

    // Initialize power arrays to zero
    std::memset(far_end_power_, 0, sizeof(far_end_power_));
    std::memset(near_end_power_, 0, sizeof(near_end_power_));
    std::memset(error_power_, 0, sizeof(error_power_));
}

AEC::~AEC() {
    // Free allocated buffers
    for (int p = 0; p < kNumPartitions; ++p) {
        delete[] filter_coeffs_[p];
    }
    delete[] far_end_history_;

    // TODO: Free pffft setup when using real pffft
}

bool AEC::Initialize() {
    LOGI("Initializing AEC: %d partitions × %d-point FFT, tail=%.1f ms",
         kNumPartitions, kFFTSize,
         static_cast<float>(kFilterLength) / sample_rate_ * 1000.0f);

    // Allocate filter coefficients (per-partition, complex)
    for (int p = 0; p < kNumPartitions; ++p) {
        filter_coeffs_[p] = new float[kNumBins * 2];  // Complex (Re, Im pairs)
        std::memset(filter_coeffs_[p], 0, kNumBins * 2 * sizeof(float));
    }

    // Allocate far-end history buffer
    far_end_history_ = new float[kFilterLength];
    std::memset(far_end_history_, 0, kFilterLength * sizeof(float));
    far_end_write_idx_ = 0;

    // TODO: Initialize pffft (when replacing stub)
    // fft_setup_ = pffft_new_setup(kFFTSize, PFFFT_REAL);

    LOGI("AEC initialized successfully");
    return true;
}

void AEC::ProcessHop(const float* near_end,
                     const float* far_end,
                     float* error_out,
                     bool is_double_talk) {
    // Simple passthrough with power-based suppression
    // Full adaptive filter will be implemented later

    // Compute signal powers
    float near_power = 0.0f;
    float far_power = 0.0f;

    for (int i = 0; i < kHopSize; ++i) {
        near_power += near_end[i] * near_end[i];
        far_power += far_end[i] * far_end[i];
    }

    near_power /= kHopSize;
    far_power /= kHopSize;

    // Simple suppression when far-end is active
    // This is a stub - provides some echo reduction without adaptation
    float suppression = 1.0f;

    if (far_power > 1e-6f && !is_double_talk) {
        // When speaker is active, apply gentle suppression
        // This prevents feedback but allows speech through
        float ratio = near_power / (far_power + 1e-6f);
        if (ratio > 1.0f) ratio = 1.0f;
        suppression = 0.5f + 0.5f * ratio;  // Range: 0.5 to 1.0
    }

    // Apply suppression and copy to output
    for (int i = 0; i < kHopSize; ++i) {
        error_out[i] = near_end[i] * suppression;
    }

    // Compute error power for ERLE
    float error_power = 0.0f;
    for (int i = 0; i < kHopSize; ++i) {
        error_power += error_out[i] * error_out[i];
    }
    error_power /= kHopSize;

    // Update ERLE estimate
    if (error_power > 1e-10f && near_power > 1e-10f) {
        float erle_linear = near_power / (error_power + 1e-10f);
        if (erle_linear < 1.0f) erle_linear = 1.0f;
        float erle_db_current = 10.0f * std::log10f(erle_linear);

        // Smooth ERLE estimate
        constexpr float erle_alpha = 0.95f;
        erle_db_ = erle_alpha * erle_db_ + (1.0f - erle_alpha) * erle_db_current;

        // Clamp to reasonable range
        if (erle_db_ < 0.0f) erle_db_ = 0.0f;
        if (erle_db_ > 30.0f) erle_db_ = 30.0f;
    } else {
        erle_db_ = 0.0f;
    }
}

void AEC::Reset() {
    LOGI("Resetting AEC filter state");

    // Clear filter coefficients
    for (int p = 0; p < kNumPartitions; ++p) {
        std::memset(filter_coeffs_[p], 0, kNumBins * 2 * sizeof(float));
    }

    // Clear far-end history
    std::memset(far_end_history_, 0, kFilterLength * sizeof(float));
    far_end_write_idx_ = 0;

    // Reset power estimates
    std::memset(far_end_power_, 0, sizeof(far_end_power_));
    std::memset(near_end_power_, 0, sizeof(near_end_power_));
    std::memset(error_power_, 0, sizeof(error_power_));

    erle_db_ = 0.0f;
}

void AEC::UpdatePowerEstimate(const float* spectrum, float* power_out) {
    // EMA smoothing: P[k] = α·P[k] + (1-α)·|X[k]|²
    for (int k = 0; k < kNumBins; ++k) {
        float re = spectrum[2 * k];
        float im = spectrum[2 * k + 1];
        float mag_sq = re * re + im * im;

        power_out[k] = kPowerSmoothingAlpha * power_out[k] +
                       (1.0f - kPowerSmoothingAlpha) * mag_sq;
    }
}

void AEC::AdaptFilter(const float* far_end_fft,
                      const float* error_fft,
                      bool is_double_talk) {
    // NLMS adaptation: W[k] += μ/(P[k] + ε) · E*[k] · X[k]
    // where E*[k] is complex conjugate of error

    float effective_step = is_double_talk ?
        step_size_ * kDoubleTalkStepScale : step_size_;

    for (int k = 0; k < kNumBins; ++k) {
        float normalization = 1.0f / (far_end_power_[k] + kMinPower);
        float mu_normalized = effective_step * normalization;

        // Complex multiplication: E*[k] · X[k]
        // E* = (e_re, -e_im), X = (x_re, x_im)
        // Product = (e_re·x_re + e_im·x_im, -e_im·x_re + e_re·x_im)
        float e_re = error_fft[2 * k];
        float e_im = error_fft[2 * k + 1];
        float x_re = far_end_fft[2 * k];
        float x_im = far_end_fft[2 * k + 1];

        float update_re = (e_re * x_re + e_im * x_im) * mu_normalized;
        float update_im = (-e_im * x_re + e_re * x_im) * mu_normalized;

        // Update filter (partition 0 for simplicity in stub)
        filter_coeffs_[0][2 * k] += update_re;
        filter_coeffs_[0][2 * k + 1] += update_im;
    }
}

float AEC::ComputeERLE() {
    // ERLE = 10·log10(P_near / P_error)
    float near_sum = 0.0f;
    float error_sum = 0.0f;

    for (int k = 0; k < kNumBins; ++k) {
        near_sum += near_end_power_[k];
        error_sum += error_power_[k];
    }

    if (error_sum < kMinPower || near_sum < kMinPower) {
        return 0.0f;
    }

    return 10.0f * std::log10f(near_sum / error_sum);
}

}  // namespace aec
}  // namespace edgeclear
