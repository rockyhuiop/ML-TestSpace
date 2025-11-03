#pragma once

#include "../rt/perf_counters.h"
#include <atomic>
#include <cstdint>

namespace edgeclear {
namespace pipeline {

/**
 * MetricsAccumulator: Aggregates real-time DSP metrics
 *
 * Tracks:
 * - ERLE (Echo Return Loss Enhancement) in dB
 * - SI-SDR (Scale-Invariant Signal-to-Distortion Ratio) delta
 * - CPU time per hop
 * - Latency (measured via loopback probe)
 * - XRun count
 * - VAD/DTD states
 *
 * Thread-safe: All reads/writes use atomic operations.
 */
class MetricsAccumulator {
public:
    MetricsAccumulator();
    ~MetricsAccumulator() = default;

    /**
     * Update metrics (called by DSP worker after each frame).
     */
    void UpdateERLE(float erle_db);
    void UpdateSISDR(float siSdr_db);
    void SetLatency(float latency_ms);
    void UpdateDTDState(int dtd_state);  // 0 = SILENCE, 1 = NEAR, 2 = FAR, 3 = DT

    /**
     * Get current metrics (thread-safe).
     */
    float GetERLE() const;
    float GetSISDR() const;
    float GetCPU() const;
    float GetLatency() const;
    uint64_t GetXRunCount() const;
    int GetVADState() const;  // 0 = INACTIVE, 1 = ACTIVE
    int GetDTDState() const;  // 0 = SILENCE, 1 = NEAR, 2 = FAR, 3 = DOUBLE_TALK

    /**
     * Set perf counters reference (for CPU/XRun tracking).
     */
    void SetPerfCounters(rt::PerfCounters* perf_counters);

    /**
     * Reset all metrics (e.g., on session restart).
     */
    void Reset();

private:
    std::atomic<float> erle_db_;
    std::atomic<float> siSdr_db_;
    std::atomic<float> latency_ms_;
    std::atomic<int> vad_state_;
    std::atomic<int> dtd_state_;

    rt::PerfCounters* perf_counters_;  // Not owned
};

}  // namespace pipeline
}  // namespace edgeclear
