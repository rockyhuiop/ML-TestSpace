#pragma once

#include "session_state.h"
#include "dsp_worker.h"
#include "metrics_accumulator.h"
#include "../audio_io/aaudio_capture.h"
#include "../audio_io/aaudio_render.h"
#include "../audio_io/audio_callback.h"
#include "../audio_io/wav_writer.h"
#include "../rt/ring_buffer.h"
#include "../rt/frame_buffer.h"
#include "../rt/perf_counters.h"
#ifdef HAVE_TFLITE
#include "../ml/denoiser_model.h"
#endif
#include <android/asset_manager.h>
#include <memory>
#include <string>
#include <atomic>

// Forward declarations for ML types when TFLite is not available
#ifndef HAVE_TFLITE
namespace edgeclear {
namespace ml {
struct DenoiserModelState;
}  // namespace ml
}  // namespace edgeclear
#endif

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
 * - Manage A/B recording (Phase 6)
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

    /**
     * Start A/B recording.
     *
     * @param raw_path Path to raw microphone WAV file
     * @param far_end_path Path to far-end reference WAV file
     * @param enhanced_path Path to enhanced output WAV file
     * @param metadata_path Path to metadata JSON file
     * @return true on success, false if already recording
     */
    bool StartRecording(const std::string& raw_path,
                        const std::string& far_end_path,
                        const std::string& enhanced_path,
                        const std::string& metadata_path);

    /**
     * Stop A/B recording and write metadata.
     *
     * @return true if recording was active
     */
    bool StopRecording();

    /**
     * Check if currently recording.
     */
    bool IsRecording() const { return is_recording_.load(std::memory_order_acquire); }
/**     * Set processing preset (Phase 7, User Story 3).     *     * Updates DSP pipeline configuration atomically:     * - Buffer sizing (hop count)     * - AEC filter length     * - Denoiser frame rate     * - RES enable/disable     *     * @param preset Preset name ("Low-latency" | "Quality" | "Battery saver")     * @return true on success, false on invalid preset     */    bool SetPreset(const std::string& preset);    /**     * Get current preset name.     */    std::string GetPreset() const { return preset_; }

    /**
     * Get WAV writers for DSP worker to write samples.
     * Returns nullptr if not recording.
     */
    audio_io::WavWriter* GetRawWriter() { return is_recording_ ? raw_writer_.get() : nullptr; }
    audio_io::WavWriter* GetFarEndWriter() { return is_recording_ ? far_end_writer_.get() : nullptr; }
    audio_io::WavWriter* GetEnhancedWriter() { return is_recording_ ? enhanced_writer_.get() : nullptr; }

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

#ifdef HAVE_TFLITE
    // Phase 5 M3: Denoiser model state
    std::unique_ptr<ml::DenoiserModelState> denoiser_model_;
#endif

    // Session state
    bool is_initialized_;
    bool is_active_;
    int32_t sample_rate_;
    int32_t hop_size_;
    std::string preset_;

    // Recording state (Phase 6)
    std::atomic<bool> is_recording_{false};
    std::unique_ptr<audio_io::WavWriter> raw_writer_;
    std::unique_ptr<audio_io::WavWriter> far_end_writer_;
    std::unique_ptr<audio_io::WavWriter> enhanced_writer_;
    std::string metadata_path_;

    /**
     * Initialize denoiser model from assets.
     * @param assetManager Android AssetManager
     * @return true if model loaded successfully (non-fatal if fails)
     */
    bool InitializeDenoiser(AAssetManager* assetManager);
};

}  // namespace pipeline
}  // namespace edgeclear
