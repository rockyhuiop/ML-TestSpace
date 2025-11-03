#include "metrics_accumulator.h"

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

}  // namespace pipeline
}  // namespace edgeclear
