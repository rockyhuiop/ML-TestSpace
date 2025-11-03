#pragma once

#include "session_state.h"
#include "dsp_worker.h"
#include "metrics_accumulator.h"
#include "../audio_io/aaudio_capture.h"
#include "../audio_io/aaudio_render.h"
#include "../audio_io/audio_callback.h"
#include "../rt/ring_buffer.h"
#include "../rt/frame_buffer.h"
#include "../rt/perf_counters.h"
#include "../ml/denoiser_model.h"
#include <android/asset_manager.h>
#include <memory>
#include <string>

namespace edgeclear {
namespace pipeline {

/**
 * SessionManager: Manages complete audio processing session
 *
 * Responsibilities:
 * - Initialize AAudio streams
 * - Create SPSC queues
 * - Start DSP worker thread
 * - Manage session lifecycle
 * - Provide metrics to JNI
 *
 * This is the main orchestrator called from JNI layer.
 */
class SessionManager {
public:
    SessionManager();
    ~SessionManager();

    // Non-copyable
    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;

    /**
     * Initialize session.
     *
     * @param sampleRate Sample rate in Hz (48000)
     * @param hopSize Hop size in samples (160 @ 16 kHz)
     * @param preset Preset name ("Low-latency" | "Quality" | "Battery saver")
     * @param assetManager Android AssetManager for loading models from assets
     * @return true on success
     */
    bool Initialize(int32_t sampleRate, int32_t hopSize, const std::string& preset,
                    AAssetManager* assetManager = nullptr);

    /**
     * Start audio processing.
     */
    bool Start();

    /**
     * Stop audio processing.
     */
    void Stop();

    /**
     * Destroy session and free resources.
     */
    void Destroy();

    /**
     * Get metrics accumulator (for JNI to read metrics).
     */
    MetricsAccumulator* GetMetrics() { return metrics_.get(); }

    /**
     * Check if session is active.
     */
    bool IsActive() const;

private:
    // AAudio streams
    std::unique_ptr<audio_io::AAudioCapture> capture_;
    std::unique_ptr<audio_io::AAudioRender> render_;

    // Audio callback handler
    std::unique_ptr<audio_io::AudioCallback> audio_callback_;

    // SPSC queues (RT capture → DSP worker → RT render)
    std::unique_ptr<rt::SPSCRingBuffer<rt::DSPFrameBuffer>> input_queue_;
    std::unique_ptr<rt::SPSCRingBuffer<rt::DSPFrameBuffer>> output_queue_;
    std::unique_ptr<rt::SPSCRingBuffer<rt::DSPFrameBuffer>> far_end_queue_;  // For AEC reference

    // DSP worker thread
    std::unique_ptr<DSPWorker> dsp_worker_;

    // Performance counters
    std::unique_ptr<rt::PerfCounters> perf_counters_;

    // Metrics accumulator
    std::unique_ptr<MetricsAccumulator> metrics_;

    // Phase 5 M3: Denoiser model state
    std::unique_ptr<ml::DenoiserModelState> denoiser_model_;

    // Session state
    bool is_initialized_;
    bool is_active_;
    int32_t sample_rate_;
    int32_t hop_size_;
    std::string preset_;

    /**
     * Initialize denoiser model from assets.
     * @param assetManager Android AssetManager
     * @return true if model loaded successfully (non-fatal if fails)
     */
    bool InitializeDenoiser(AAssetManager* assetManager);
};

}  // namespace pipeline
}  // namespace edgeclear
