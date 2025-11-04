#include "metrics_accumulator.h"
#include <android/log.h>
#include <string>

namespace edgeclear {
namespace pipeline {

MetricsAccumulator::MetricsAccumulator()
    : erle_db_(0.0f),
      siSdr_db_(0.0f),
      latency_ms_(0.0f),
      vad_state_(0),
      dtd_state_(1),  // Default: NEAR_END_ONLY
      perf_counters_(nullptr) {
}

void MetricsAccumulator::UpdateERLE(float erle_db) {
    erle_db_.store(erle_db, std::memory_order_relaxed);
}

void MetricsAccumulator::UpdateSISDR(float siSdr_db) {
    siSdr_db_.store(siSdr_db, std::memory_order_relaxed);
}

void MetricsAccumulator::SetLatency(float latency_ms) {
    latency_ms_.store(latency_ms, std::memory_order_relaxed);
}

void MetricsAccumulator::UpdateDTDState(int dtd_state) {
    dtd_state_.store(dtd_state, std::memory_order_relaxed);
}

float MetricsAccumulator::GetERLE() const {
    return erle_db_.load(std::memory_order_acquire);
}

float MetricsAccumulator::GetSISDR() const {
    return siSdr_db_.load(std::memory_order_acquire);
}

float MetricsAccumulator::GetCPU() const {
    return perf_counters_ ? perf_counters_->GetCpuTimeMs() : 0.0f;
}

float MetricsAccumulator::GetLatency() const {
    return latency_ms_.load(std::memory_order_acquire);
}

uint64_t MetricsAccumulator::GetXRunCount() const {
    return perf_counters_ ? perf_counters_->GetXRunCount() : 0;
}

int MetricsAccumulator::GetVADState() const {
    return vad_state_.load(std::memory_order_acquire);
}

int MetricsAccumulator::GetDTDState() const {
    return dtd_state_.load(std::memory_order_acquire);
}

void MetricsAccumulator::SetPerfCounters(rt::PerfCounters* perf_counters) {
    perf_counters_ = perf_counters;
}

void MetricsAccumulator::Reset() {
    erle_db_.store(0.0f, std::memory_order_release);
    siSdr_db_.store(0.0f, std::memory_order_release);
    latency_ms_.store(0.0f, std::memory_order_release);
    vad_state_.store(0, std::memory_order_release);
    dtd_state_.store(1, std::memory_order_release);

    if (perf_counters_) {
        perf_counters_->Reset();
    }
}

void MetricsAccumulator::LogPresetChange(const std::string& preset_name) {
    // Phase 7 T093/T094: Log preset change with expected performance impact
    
    // Expected impacts from plan.md performance budgets:
    // Low-latency: 25 ms latency, 5 ms CPU, tight buffers
    // Quality: 38 ms latency, 6 ms CPU, full processing
    // Battery saver: 35 ms latency, 4.5 ms CPU (denoiser /2, RES off)
    
    float expected_latency_ms = 38.0f;  // Default to Quality
    float expected_cpu_ms = 6.0f;
    
    if (preset_name == "Low-latency") {
        expected_latency_ms = 25.0f;
        expected_cpu_ms = 5.0f;
        __android_log_print(ANDROID_LOG_INFO, "EdgeClear-Metrics",
            "Preset changed: Low-latency (target: 25 ms latency, ~5 ms CPU)");
        __android_log_print(ANDROID_LOG_INFO, "EdgeClear-Metrics",
            "  - 1 hop buffer (tight)");
        __android_log_print(ANDROID_LOG_INFO, "EdgeClear-Metrics",
            "  - 4 AEC partitions (85 ms tail)");
        __android_log_print(ANDROID_LOG_INFO, "EdgeClear-Metrics",
            "  - Denoiser every hop, RES off");
    } else if (preset_name == "Quality") {
        expected_latency_ms = 38.0f;
        expected_cpu_ms = 6.0f;
        __android_log_print(ANDROID_LOG_INFO, "EdgeClear-Metrics",
            "Preset changed: Quality (target: 38 ms latency, ~6 ms CPU)");
        __android_log_print(ANDROID_LOG_INFO, "EdgeClear-Metrics",
            "  - 2 hop buffer (safe)");
        __android_log_print(ANDROID_LOG_INFO, "EdgeClear-Metrics",
            "  - 8 AEC partitions (170 ms tail)");
        __android_log_print(ANDROID_LOG_INFO, "EdgeClear-Metrics",
            "  - Denoiser every hop, RES on");
    } else if (preset_name == "Battery saver") {
        expected_latency_ms = 35.0f;
        expected_cpu_ms = 4.5f;
        __android_log_print(ANDROID_LOG_INFO, "EdgeClear-Metrics",
            "Preset changed: Battery saver (target: 35 ms latency, ~4.5 ms CPU)");
        __android_log_print(ANDROID_LOG_INFO, "EdgeClear-Metrics",
            "  - 2 hop buffer (safe)");
        __android_log_print(ANDROID_LOG_INFO, "EdgeClear-Metrics",
            "  - 8 AEC partitions (170 ms tail)");
        __android_log_print(ANDROID_LOG_INFO, "EdgeClear-Metrics",
            "  - Denoiser every 2nd hop (-1.5 ms), RES off (-0.2 ms)");
    } else {
        __android_log_print(ANDROID_LOG_WARN, "EdgeClear-Metrics",
            "Unknown preset: %s", preset_name.c_str());
    }
    
    // Log current vs expected (for validation)
    float current_latency = latency_ms_.load(std::memory_order_acquire);
    float current_cpu = perf_counters_ ? perf_counters_->GetCpuTimeMs() : 0.0f;
    
    __android_log_print(ANDROID_LOG_INFO, "EdgeClear-Metrics",
        "Performance: Current latency=%.1f ms (target %.1f ms), CPU=%.2f ms (target %.1f ms)",
        current_latency, expected_latency_ms, current_cpu, expected_cpu_ms);
}

}  // namespace pipeline
}  // namespace edgeclear
