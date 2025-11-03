#pragma once

#include <atomic>
#include <cstdint>
#include <time.h>

namespace edgeclear {
namespace rt {

/**
 * PerfCounters: Performance monitoring for RT audio processing
 *
 * Tracks:
 * - CPU time per hop (target <6 ms per 10 ms hop)
 * - XRuns (buffer underflow/overflow events)
 * - Processing latency
 *
 * Thread-safe: Atomic operations for concurrent read/write.
 */
class PerfCounters {
public:
    PerfCounters()
        : cpu_time_us_(0),
          xrun_count_(0),
          frames_processed_(0) {
    }

    /**
     * Get current monotonic time in microseconds.
     * Uses CLOCK_MONOTONIC for stable measurements (not affected by system time changes).
     */
    static inline uint64_t GetTimeUs() {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return static_cast<uint64_t>(ts.tv_sec) * 1000000ULL +
               static_cast<uint64_t>(ts.tv_nsec) / 1000ULL;
    }

    /**
     * ScopedTimer: RAII timer for measuring function execution time.
     *
     * Usage:
     *   {
     *     ScopedTimer timer(perf_counters);
     *     // ... DSP processing ...
     *   }  // Destructor updates cpu_time_us_
     */
    class ScopedTimer {
    public:
        explicit ScopedTimer(PerfCounters& counters)
            : counters_(counters),
              start_time_(GetTimeUs()) {
        }

        ~ScopedTimer() {
            const uint64_t end_time = GetTimeUs();
            const uint64_t elapsed_us = end_time - start_time_;

            // Update exponential moving average: α = 0.1
            // EMA = α * new_sample + (1 - α) * EMA
            const uint64_t prev_cpu = counters_.cpu_time_us_.load(std::memory_order_relaxed);
            const uint64_t new_cpu = (elapsed_us + 9 * prev_cpu) / 10;
            counters_.cpu_time_us_.store(new_cpu, std::memory_order_relaxed);
        }

    private:
        PerfCounters& counters_;
        const uint64_t start_time_;
    };

    /**
     * Increment XRun counter (called on buffer overflow/underflow).
     */
    void IncrementXRun() {
        xrun_count_.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * Increment frames processed counter.
     */
    void IncrementFrames() {
        frames_processed_.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * Get CPU time per hop in milliseconds.
     * Thread-safe: Atomic load.
     */
    float GetCpuTimeMs() const {
        return cpu_time_us_.load(std::memory_order_acquire) / 1000.0f;
    }

    /**
     * Get total XRun count since session start.
     * Thread-safe: Atomic load.
     */
    uint64_t GetXRunCount() const {
        return xrun_count_.load(std::memory_order_acquire);
    }

    /**
     * Get total frames processed since session start.
     */
    uint64_t GetFramesProcessed() const {
        return frames_processed_.load(std::memory_order_acquire);
    }

    /**
     * Reset all counters (e.g., on session restart).
     */
    void Reset() {
        cpu_time_us_.store(0, std::memory_order_release);
        xrun_count_.store(0, std::memory_order_release);
        frames_processed_.store(0, std::memory_order_release);
    }

private:
    std::atomic<uint64_t> cpu_time_us_;        // EMA of CPU time in microseconds
    std::atomic<uint64_t> xrun_count_;         // Cumulative XRun count
    std::atomic<uint64_t> frames_processed_;   // Cumulative frame count
};

}  // namespace rt
}  // namespace edgeclear
