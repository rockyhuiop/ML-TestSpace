#pragma once

#include "../rt/ring_buffer.h"
#include "../rt/frame_buffer.h"
#include "../rt/perf_counters.h"
#include "../dsp/stft.h"
#include "../dsp/istft.h"
#include "../aec/aec.h"
#include "../aec/dtd.h"
#include "../aec/res.h"
#ifdef HAVE_TFLITE
#include "../ml/denoiser_model.h"
#include "../ml/denoiser_inference.h"
#endif
#include <thread>
#include <atomic>
#include <memory>

// Forward declarations for ML types when TFLite is not available
#ifndef HAVE_TFLITE
namespace edgeclear {
namespace ml {
struct DenoiserModelState;
struct DenoiserInference;
}  // namespace ml
}  // namespace edgeclear
#endif

namespace edgeclear {

// Forward declaration for WAV writer
namespace audio_io {
class WavWriter;
}

namespace pipeline{

/**
 * DSPWorker: Non-RT DSP processing thread
 *
 * Processes audio frames from SPSC input queue:
 * - Dequeue frame from input queue
 * - Process through DSP pipeline (STFT → AEC → RES → Denoiser → ISTFT)
 * - Enqueue to output queue
 * - Write samples to WAV files if recording (Phase 6)
 *
 * Constitution requirements:
 * - Complete processing in <6 ms per 10 ms hop (60% duty cycle)
 * - Track CPU time and update metrics
 * - No blocking operations
 */
class DSPWorker {
public:
    DSPWorker(rt::SPSCRingBuffer<rt::DSPFrameBuffer>* input_queue,
              rt::SPSCRingBuffer<rt::DSPFrameBuffer>* output_queue,
              rt::SPSCRingBuffer<rt::DSPFrameBuffer>* far_end_queue,
              rt::PerfCounters* perf_counters,
              class MetricsAccumulator* metrics = nullptr,
              ml::DenoiserModelState* denoiser_model = nullptr);

    ~DSPWorker();

    // Non-copyable
    DSPWorker(const DSPWorker&) = delete;
    DSPWorker& operator=(const DSPWorker&) = delete;

    /**
     * Start DSP worker thread.
     */
    bool Start();

    /**
     * Stop DSP worker thread.
     */
    void Stop();

    /**
     * Check if worker is running.
     */
    bool IsRunning() const { return is_running_.load(std::memory_order_acquire); }

    /**
     * Enable/disable AEC processing.
     */
    void SetAECEnabled(bool enabled) { aec_enabled_ = enabled; }

#ifdef HAVE_TFLITE
    /**
     * Enable/disable denoiser processing.
     */
    void SetDenoiserEnabled(bool enabled) { denoiser_enabled_ = enabled; }
#endif
    /**
     * Set denoiser decimation rate (Phase 7 Task T092).
     *
     * Decimation = 1: Process every hop (default, Quality preset)
     * Decimation = 2: Process every 2nd hop (Battery saver preset)
     *
     * Saves ~1.5 ms CPU per skipped hop.
     */
    void SetDenoiserDecimation(int decimation) { denoiser_decimation_ = decimation; }

    /**
     * Enable/disable RES post-filter (Phase 7 Task T092).
     *
     * RES provides +3 dB ERLE but costs ~0.2 ms CPU.
     * Enabled in Quality preset, disabled in Low-latency and Battery saver.
     */
    void SetRESEnabled(bool enabled) { res_enabled_ = enabled; }

    /**
     * Get current DTD state.
     */
    aec::DTD::State GetDTDState() const {
        return dtd_ ? dtd_->GetState() : aec::DTD::SILENCE;
    }

    /**
     * Set WAV writers for recording (Phase 6).
     * Pass nullptr to disable recording.
     */
    void SetRecordingWriters(audio_io::WavWriter* raw_writer,
                              audio_io::WavWriter* far_end_writer,
                              audio_io::WavWriter* enhanced_writer) {
        raw_writer_ = raw_writer;
        far_end_writer_ = far_end_writer;
        enhanced_writer_ = enhanced_writer;
    }

private:
    /**
     * Worker thread main loop.
     */
    void WorkerLoop();

    /**
     * Process one frame through DSP pipeline.
     */
    void ProcessFrame(const rt::DSPFrameBuffer& input, rt::DSPFrameBuffer& output);

    rt::SPSCRingBuffer<rt::DSPFrameBuffer>* input_queue_;
    rt::SPSCRingBuffer<rt::DSPFrameBuffer>* output_queue_;
    rt::SPSCRingBuffer<rt::DSPFrameBuffer>* far_end_queue_;  // AEC reference
    rt::PerfCounters* perf_counters_;
    class MetricsAccumulator* metrics_;  // Not owned

    std::unique_ptr<std::thread> worker_thread_;
    std::atomic<bool> is_running_;

    // DSP modules (allocated on heap to avoid stack overflow)
    std::unique_ptr<dsp::STFT> stft_error_;
    std::unique_ptr<dsp::STFT> stft_near_;
    std::unique_ptr<dsp::STFT> stft_far_;
    std::unique_ptr<dsp::ISTFT> istft_;

    // Phase 4 M2: AEC modules
    std::unique_ptr<aec::AEC> aec_;
    std::unique_ptr<aec::DTD> dtd_;
    std::unique_ptr<aec::RES> res_;
    bool aec_enabled_;
    aec::DTD::State previous_dtd_state_{aec::DTD::SILENCE};  // DTD state from previous frame

    // Phase 7: Preset control variables (used unconditionally)
    int denoiser_decimation_{1};   // Denoiser decimation rate (1 or 2)
    int frame_counter_{0};         // Frame counter for decimation
    bool res_enabled_{true};       // RES enable/disable flag

#ifdef HAVE_TFLITE
    // Phase 5 M3: Denoiser modules
    ml::DenoiserModelState* denoiser_model_;  // Not owned (managed by SessionManager)
    std::unique_ptr<ml::DenoiserInference> denoiser_;
    bool denoiser_enabled_;
#endif

    // Phase 6: Recording WAV writers (not owned, managed by SessionManager)
    audio_io::WavWriter* raw_writer_ = nullptr;
    audio_io::WavWriter* far_end_writer_ = nullptr;
    audio_io::WavWriter* enhanced_writer_ = nullptr;

    // Processing buffers (pre-allocated, reused across frames)
    float near_spectrum_[322];   // Near-end FFT (161 complex bins = 322 floats)
    float far_spectrum_[322];    // Far-end FFT (161 complex bins = 322 floats)
    float error_spectrum_[322];  // AEC error FFT (161 complex bins = 322 floats)
    float near_time_[320];       // Near-end time domain (320 samples for STFT overlap buffer)
    float far_time_[320];        // Far-end time domain
    float output_time_[160];     // Output time domain (one hop = 160 samples)
    float magnitude_[161];       // Magnitude spectrum for denoiser input (161 bins)
    float gain_[161];            // Gain values from denoiser output (161 bins)
};

}  // namespace pipeline
}  // namespace edgeclear
