#pragma once

#include <cstddef>
#include <cstdint>

namespace edgeclear {
namespace dsp {

/**
 * FDNLMS_AEC: Frequency-Domain Normalized Least Mean Squares Acoustic Echo Canceller
 *
 * Partitioned-block frequency-domain adaptive filter for echo cancellation.
 * See research.md R6 for partitioning strategy and performance analysis.
 *
 * Key parameters:
 * - Partitions: 4 (85 ms), 8 (170 ms), or 16 (340 ms) echo tail coverage
 * - Partition size: 1024 complex samples per partition
 * - Step size μ: 0.3 (FD-NLMS adaptation rate)
 * - Epsilon ε: 1e-6 (power normalization regularization)
 *
 * Constitution compliance:
 * - Fixed allocations at initialization (zero allocations in ProcessFrame)
 * - Deterministic behavior (bit-exact output)
 * - CPU budget: ~1 ms per hop for 8 partitions
 */
class FDNLMS_AEC {
public:
    /**
     * Construct AEC with specified filter length.
     *
     * @param num_partitions Number of partitions (4, 8, or 16)
     *                       4 = 85 ms tail, 8 = 170 ms tail, 16 = 340 ms tail
     */
    explicit FDNLMS_AEC(int num_partitions = 8);
    ~FDNLMS_AEC() = default;

    // Non-copyable (large memory footprint)
    FDNLMS_AEC(const FDNLMS_AEC&) = delete;
    FDNLMS_AEC& operator=(const FDNLMS_AEC&) = delete;

    /**
     * Process one frame of audio (time-domain).
     *
     * @param near Near-end microphone input (160 samples @ 16 kHz)
     * @param far Far-end reference signal (160 samples @ 16 kHz)
     * @param out Echo-cancelled output (160 samples @ 16 kHz)
     * @param size Frame size in samples (must be 160)
     */
    void ProcessFrame(const float* near, const float* far, float* out, int size);

    /**
     * Reset AEC filter state (e.g., on echo path change).
     */
    void Reset();

    /**
     * Get current filter length in partitions.
     */
    int GetNumPartitions() const { return num_partitions_; }

    /**
     * Get echo tail length in milliseconds.
     * Formula: (partitions * 1024 samples) / 16000 Hz * 1000 ms/s
     */
    float GetEchoTailMs() const {
        return (num_partitions_ * 1024.0f * 1000.0f) / 16000.0f;
    }

    /**
     * Set AEC filter length (Phase 7 Task T091).
     *
     * NOTE: Changing filter length at runtime requires reinitialization of
     * internal buffers, which violates Constitution zero-allocation rule.
     *
     * For MVP, filter length changes require session restart:
     * - Quality ↔ Battery saver: No change needed (both use 8 partitions)
     * - Low-latency ↔ others: Requires restart (4 vs 8 partitions)
     *
     * @param num_partitions New partition count (4, 8, or 16)
     * @return true if applied (same value), false if restart required
     */
    bool SetNumPartitions(int num_partitions);

private:
    int num_partitions_{8};
    bool is_initialized_{false};

    // TODO Phase 4 M2: Implement actual FD-NLMS algorithm
    // - Partition buffers (num_partitions × 1024 complex float)
    // - Filter coefficients
    // - DTD (double-talk detector)
    // - Power normalization
    // See aec_filter_state.h for detailed state management
};

}  // namespace dsp
}  // namespace edgeclear
