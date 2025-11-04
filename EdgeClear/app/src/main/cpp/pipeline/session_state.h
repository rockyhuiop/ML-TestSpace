#pragma once
#include <cstdint>
#include <atomic>

namespace edgeclear {
namespace pipeline {

/**
 * SessionState: Shared session state and component flags
 *
 * Component enable flags use std::atomic<bool> with memory_order_release (store)
 * and memory_order_acquire (load) for thread-safe communication between UI thread
 * and DSP worker thread (Phase 8 T099).
 */
struct SessionState {
    uint64_t session_id;
    bool is_active;

    // Phase 8: Component enable flags (atomic for thread-safe toggle)
    std::atomic<bool> aec_enabled{true};       // AEC enabled by default
    std::atomic<bool> res_enabled{true};       // RES enabled by default
    std::atomic<bool> denoiser_enabled{true};  // Denoiser enabled by default
    std::atomic<bool> av_vad_enabled{false};   // AV-VAD disabled by default (Phase 9)
};

}  // namespace pipeline
}  // namespace edgeclear
