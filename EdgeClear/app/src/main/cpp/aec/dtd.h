#pragma once

#include <cstdint>

namespace edgeclear {
namespace aec {

/**
 * DTD: Coherence-based double-talk detector
 *
 * Detects four states:
 * - SILENCE: No activity
 * - NEAR_END_ONLY: User speaking (microphone)
 * - FAR_END_ONLY: Far-end playing (loudspeaker)
 * - DOUBLE_TALK: Both active simultaneously
 *
 * Algorithm:
 * - Computes Magnitude Squared Coherence (MSC) between near/far signals
 * - MSC[k] = |Pxy[k]|² / (Pxx[k] · Pyy[k])
 * - High MSC (>0.8) indicates correlation → FAR_END_ONLY
 * - Low MSC (<0.5) + near activity → NEAR_END_ONLY
 * - Low MSC + both active → DOUBLE_TALK
 * - Hangover counter prevents rapid state switching
 *
 * Constitution compliance:
 * - RT-safe: No allocations
 * - Low CPU: Simple power/correlation tracking
 *
 * References:
 * - Benesty et al. (2001): "On the Use of the Cepstrum for Double-Talk Detection"
 * - ITU-T G.168: Echo canceller characteristics
 */
class DTD {
public:
    static constexpr int kNumBins = 161;  // 320-point FFT / 2 + 1
    static constexpr int kHangoverFrames = 5;  // 50 ms @ 10 ms/frame

    enum State {
        SILENCE = 0,
        NEAR_END_ONLY = 1,
        FAR_END_ONLY = 2,
        DOUBLE_TALK = 3
    };

    DTD();
    ~DTD() = default;

    /**
     * Process one frame and update state.
     *
     * @param near_fft Near-end spectrum (complex, 161 bins)
     * @param far_fft Far-end spectrum (complex, 161 bins)
     * @param error_fft AEC error spectrum (complex, 161 bins)
     * @return Current DTD state
     */
    State ProcessFrame(const float* near_fft,
                       const float* far_fft,
                       const float* error_fft);

    /**
     * Get current state.
     */
    State GetState() const { return state_; }

    /**
     * Check if currently in double-talk.
     */
    bool IsDoubleTalk() const {
        return state_ == DOUBLE_TALK;
    }

    /**
     * Reset detector state.
     */
    void Reset();

private:
    // Current state
    State state_;

    // Hangover counter (prevents rapid switching)
    int hangover_counter_;

    // Power estimates (EMA smoothed)
    float near_power_[kNumBins];
    float far_power_[kNumBins];
    float error_power_[kNumBins];

    // Cross-correlation estimate
    float cross_real_[kNumBins];  // Re(Pxy)
    float cross_imag_[kNumBins];  // Im(Pxy)

    // Thresholds
    static constexpr float kSilenceThreshold = 1e-6f;
    static constexpr float kMSCHighThreshold = 0.8f;  // Correlated
    static constexpr float kMSCLowThreshold = 0.5f;   // Uncorrelated
    static constexpr float kPowerSmoothingAlpha = 0.9f;

    // Internal methods
    void UpdatePowerEstimates(const float* near_fft,
                              const float* far_fft,
                              const float* error_fft);
    float ComputeMSC();
    State DetermineState(float msc, float near_power, float far_power);
};

}  // namespace aec
}  // namespace edgeclear
