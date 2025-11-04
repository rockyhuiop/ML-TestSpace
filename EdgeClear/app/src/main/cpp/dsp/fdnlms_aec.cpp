// fdnlms_aec.cpp - AEC implementation (Phase 4 M2 stub + Phase 7 preset support)
#include "fdnlms_aec.h"
#include <android/log.h>
#include <cstring>

#define LOG_TAG "EdgeClear-AEC"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace edgeclear {
namespace dsp {

FDNLMS_AEC::FDNLMS_AEC(int num_partitions)
    : num_partitions_(num_partitions),
      is_initialized_(false) {

    // Validate partition count
    if (num_partitions != 4 && num_partitions != 8 && num_partitions != 16) {
        LOGE("Invalid partition count: %d (must be 4, 8, or 16), defaulting to 8",
             num_partitions);
        num_partitions_ = 8;
    }

    LOGI("AEC initialized with %d partitions (%.1f ms tail)", 
         num_partitions_, GetEchoTailMs());

    // TODO Phase 4 M2: Allocate partition buffers, filter coefficients
    // Memory footprint: num_partitions × 1024 × sizeof(complex float) × 2 (near+far)
    //                 = num_partitions × 1024 × 8 × 2 ≈ 16 KB per partition
    //                 = 128 KB for 8 partitions, 256 KB for 16 partitions

    is_initialized_ = true;
}

void FDNLMS_AEC::ProcessFrame(const float* near, const float* far, float* out, int size) {
    // Phase 4 M2 stub: Pass through near signal
    // TODO: Implement partitioned-block FD-NLMS algorithm
    // 1. Transform near/far to frequency domain (STFT)
    // 2. Compute error signal: E[k] = Near[k] - W[k] * Far[k]
    // 3. Update filter: W[k] = W[k] + μ * E[k] * conj(Far[k]) / (ε + |Far[k]|²)
    // 4. Transform error back to time domain (ISTFT)

    if (size != 160) {
        LOGE("Invalid frame size: %d (expected 160)", size);
        std::memset(out, 0, size * sizeof(float));
        return;
    }

    // Stub: copy near to out (no echo cancellation yet)
    std::memcpy(out, near, size * sizeof(float));
}

void FDNLMS_AEC::Reset() {
    LOGI("Resetting AEC filter state");
    
    // TODO Phase 4 M2: Reset filter coefficients to zero
    // Reset history buffers
    // Reset DTD state
}

bool FDNLMS_AEC::SetNumPartitions(int num_partitions) {
    // Validate partition count
    if (num_partitions != 4 && num_partitions != 8 && num_partitions != 16) {
        LOGE("Invalid partition count: %d (must be 4, 8, or 16)", num_partitions);
        return false;
    }

    // If same value, no-op
    if (num_partitions == num_partitions_) {
        LOGI("AEC partitions unchanged: %d", num_partitions_);
        return true;
    }

    // Changing partition count requires reallocation → violates Constitution
    // Return false to signal that session restart is required
    LOGI("AEC partition change requested: %d → %d (requires session restart)",
         num_partitions_, num_partitions);
    
    return false;  // Indicates restart required
}

}  // namespace dsp
}  // namespace edgeclear
