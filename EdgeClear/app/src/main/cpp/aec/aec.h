#pragma once

#include <cstdint>
#include <memory>

namespace edgeclear {
namespace aec {

/**
 * AEC: Partitioned-block frequency-domain NLMS adaptive filter
 *
 * Architecture:
 * - 8 partitions × 320-point FFT (160 ms echo tail @ 16 kHz)
 * - Overlap-save processing (160 hop size)
 * - Normalized step size with power estimation
 * - Constrained adaptation during double-talk
 *
 * Constitution compliance:
 * - RT-safe: No allocations in ProcessHop()
 * - Deterministic: Fixed buffer sizes
 * - SIMD-friendly: Aligned memory, vectorizable loops
 *
 * Target performance:
 * - ERLE: 12-18 dB on music far-end
 * - CPU: <4 ms per hop (within 6 ms budget)
 *
 * References:
 * - Sondhi & Berkley (1980): Stereophonic AEC
 * - Gay & Tavathia (1995): Subband AEC
 * - research.md: R1 (AEC/DTD fusion)
 *
 * Note: Using 512-point FFT to match STFT/ISTFT pipeline
 */
class AEC {
public:
    static constexpr int kFFTSize = 320;
    static constexpr int kHopSize = 160;
    static constexpr int kNumPartitions = 8;
    static constexpr int kFilterLength = kFFTSize * kNumPartitions;  // 2560 taps = 160 ms @ 16 kHz
    static constexpr int kNumBins = kFFTSize / 2 + 1;  // 161 bins

    /**
     * Constructor.
     *
     * @param sampleRate Sample rate (must be 16000)
     * @param stepSize Adaptation step size (typical: 0.3-0.5)
     */
    explicit AEC(int32_t sampleRate, float stepSize = 0.4f);
    ~AEC();

    // Non-copyable, non-movable (contains pffft state)
    AEC(const AEC&) = delete;
    AEC& operator=(const AEC&) = delete;
    AEC(AEC&&) = delete;
    AEC& operator=(AEC&&) = delete;

    /**
     * Initialize AEC (allocates buffers).
     *
     * @return true on success
     */
    bool Initialize();

    /**
     * Process one hop (160 samples @ 16 kHz = 10 ms).
     *
     * @param near_end Input near-end signal (microphone)
     * @param far_end Input far-end signal (loudspeaker reference)
     * @param error_out Output error signal (AEC residual)
     * @param is_double_talk true if DTD detects double-talk
     */
    void ProcessHop(const float* near_end,
                    const float* far_end,
                    float* error_out,
                    bool is_double_talk);

    /**
     * Reset filter state (on far-end discontinuity).
     */
    void Reset();

    /**
     * Get current ERLE estimate (dB).
     */
    float GetERLE() const { return erle_db_; }

private:
    // Configuration
    int32_t sample_rate_;
    float step_size_;

    // Filter coefficients (frequency domain, 8 partitions × 161 bins)
    // W[p][k] = complex filter coefficient for partition p, bin k
    // Storage: [Re(0), Im(0), Re(1), Im(1), ...] per partition
    float* filter_coeffs_[kNumPartitions];  // 8 × 161 complex = ~5 KB

    // Far-end history buffer (time domain, circular)
    // Stores last kFilterLength samples for partitioned convolution
    float* far_end_history_;  // 2560 samples = 10 KB
    int far_end_write_idx_;

    // Power estimation buffers
    float far_end_power_[kNumBins];  // Per-bin power (EMA smoothed)
    float near_end_power_[kNumBins];
    float error_power_[kNumBins];

    // ERLE tracking
    float erle_db_;

    // pffft workspace (TODO: replace stub)
    [[maybe_unused]] void* fft_setup_;

    // Internal methods
    void UpdatePowerEstimate(const float* spectrum, float* power_out);
    void AdaptFilter(const float* far_end_fft,
                     const float* error_fft,
                     bool is_double_talk);
    float ComputeERLE();
};

}  // namespace aec
}  // namespace edgeclear
