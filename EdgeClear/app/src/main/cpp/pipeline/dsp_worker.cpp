#include "dsp_worker.h"
#include "metrics_accumulator.h"
#include "audio_io/wav_writer.h"
#include <android/log.h>
#include <cstring>
#include <cmath>

#define LOG_TAG "EdgeClear-DSPWorker"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace edgeclear {
namespace pipeline {

DSPWorker::DSPWorker(
        rt::SPSCRingBuffer<rt::DSPFrameBuffer>* input_queue,
        rt::SPSCRingBuffer<rt::DSPFrameBuffer>* output_queue,
        rt::SPSCRingBuffer<rt::DSPFrameBuffer>* far_end_queue,
        rt::PerfCounters* perf_counters,
        MetricsAccumulator* metrics,
        ml::DenoiserModelState* denoiser_model)
    : input_queue_(input_queue),
      output_queue_(output_queue),
      far_end_queue_(far_end_queue),
      perf_counters_(perf_counters),
      metrics_(metrics),
      is_running_(false),
      aec_enabled_(true)  // Enable AEC by default
#ifdef HAVE_TFLITE
      ,denoiser_model_(denoiser_model),
      denoiser_enabled_(true)  // Denoiser enabled by default (when model available)
#endif
      {

    // Allocate DSP modules (separate STFT instances to avoid overlap state corruption)
    stft_error_ = std::make_unique<dsp::STFT>();
    stft_near_ = std::make_unique<dsp::STFT>();
    stft_far_ = std::make_unique<dsp::STFT>();
    istft_ = std::make_unique<dsp::ISTFT>();

    // Phase 4 M2: Allocate AEC modules
    aec_ = std::make_unique<aec::AEC>(16000);  // 16 kHz sample rate
    dtd_ = std::make_unique<aec::DTD>();
    res_ = std::make_unique<aec::RES>();

    // Initialize AEC
    if (!aec_->Initialize()) {
        LOGE("Failed to initialize AEC - disabling");
        aec_enabled_ = false;
    }

#ifdef HAVE_TFLITE
    // Phase 5 M3: Initialize denoiser (if model provided)
    if (denoiser_model_ && denoiser_model_->IsReady()) {
        denoiser_ = std::make_unique<ml::DenoiserInference>();
        if (denoiser_->Initialize(denoiser_model_)) {
            denoiser_enabled_ = true;
            LOGI("Denoiser initialized and enabled");
        } else {
            LOGE("Failed to initialize denoiser - disabling");
            denoiser_enabled_ = false;
        }
    } else {
        LOGI("Denoiser not available (no model provided)");
        denoiser_enabled_ = false;
    }
#else
    (void)denoiser_model;  // Suppress unused parameter warning
    LOGI("TensorFlow Lite not available - denoiser disabled");
#endif

    // Zero processing buffers
    std::memset(near_spectrum_, 0, sizeof(near_spectrum_));
    std::memset(far_spectrum_, 0, sizeof(far_spectrum_));
    std::memset(error_spectrum_, 0, sizeof(error_spectrum_));
    std::memset(near_time_, 0, sizeof(near_time_));
    std::memset(far_time_, 0, sizeof(far_time_));
    std::memset(output_time_, 0, sizeof(output_time_));
    std::memset(magnitude_, 0, sizeof(magnitude_));
    std::memset(gain_, 0, sizeof(gain_));

    LOGI("DSPWorker initialized (AEC %s, Denoiser %s)",
         aec_enabled_ ? "enabled" : "disabled",
#ifdef HAVE_TFLITE
         denoiser_enabled_ ? "enabled" : "disabled"
#else
         "not available (TFLite disabled)"
#endif
         );
}

DSPWorker::~DSPWorker() {
    Stop();
    LOGI("DSPWorker destroyed");
}

bool DSPWorker::Start() {
    if (is_running_.load(std::memory_order_acquire)) {
        LOGE("DSPWorker already running");
        return false;
    }

    is_running_.store(true, std::memory_order_release);

    // Create worker thread
    worker_thread_ = std::make_unique<std::thread>(&DSPWorker::WorkerLoop, this);

    LOGI("DSPWorker thread started");
    return true;
}

void DSPWorker::Stop() {
    if (!is_running_.load(std::memory_order_acquire)) {
        return;
    }

    is_running_.store(false, std::memory_order_release);

    // Wait for worker thread to exit
    if (worker_thread_ && worker_thread_->joinable()) {
        worker_thread_->join();
    }

    LOGI("DSPWorker thread stopped");
}

void DSPWorker::WorkerLoop() {
    LOGI("DSPWorker loop starting");

    int pop_success_count = 0;
    int pop_fail_count = 0;
    int push_fail_count = 0;

    while (is_running_.load(std::memory_order_acquire)) {
        rt::DSPFrameBuffer input_frame;

        // Try to dequeue input frame
        if (input_queue_->Pop(input_frame)) {
            pop_success_count++;

            // Log every 100 successful pops
            if (pop_success_count % 100 == 0) {
                float cpu_ms = perf_counters_->GetCpuTimeMs();
                LOGI("DSPWorker: Processed %d frames (pop_fails=%d, push_fails=%d, cpu=%.2f ms)",
                     pop_success_count, pop_fail_count, push_fail_count, cpu_ms);
            }

            // Start CPU timer
            uint64_t start_time = rt::PerfCounters::GetTimeUs();
            rt::PerfCounters::ScopedTimer timer(*perf_counters_);

            // Process frame
            rt::DSPFrameBuffer output_frame;
            ProcessFrame(input_frame, output_frame);

            // Log actual timing for first few frames
            if (pop_success_count <= 5) {
                uint64_t elapsed = rt::PerfCounters::GetTimeUs() - start_time;
                LOGI("Frame %d: Processing took %llu microseconds (%.2f ms)",
                     pop_success_count, (unsigned long long)elapsed, elapsed / 1000.0f);
            }

            // Enqueue output frame (if full, drop frame to prevent blocking)
            if (!output_queue_->Push(output_frame)) {
                // Output queue full - render callback is too slow
                // Drop this frame to prevent blocking the DSP worker
                push_fail_count++;
                if (push_fail_count <= 20 || push_fail_count % 100 == 0) {
                    LOGE("Output queue overflow - dropping frame (total drops: %d)", push_fail_count);
                }
            }

            // Increment frame counter
            perf_counters_->IncrementFrames();
        } else {
            pop_fail_count++;

            // Log if we're consistently failing to pop
            if (pop_fail_count == 1000 || pop_fail_count == 10000 || pop_fail_count % 50000 == 0) {
                LOGE("DSPWorker: Input queue empty for %d consecutive attempts (processed=%d frames)",
                     pop_fail_count, pop_success_count);
            }

            // No input available - sleep briefly to avoid spinning
            // This is non-RT thread, so sleeping is acceptable
            // Use very short sleep (100μs) for better power efficiency
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }

    LOGI("DSPWorker loop exiting (processed=%d, pop_fails=%d, push_fails=%d)",
         pop_success_count, pop_fail_count, push_fail_count);
}

void DSPWorker::ProcessFrame(const rt::DSPFrameBuffer& input,
                              rt::DSPFrameBuffer& output) {
    // Phase 7 T092: Track frame count for denoiser decimation    frame_counter_++;
    // Phase 6: Capture raw input for recording (before any processing)
    if (raw_writer_) {
        raw_writer_->WriteSamples(input.samples, rt::DSPFrameBuffer::kFrameSize);
    }

    // Convert int16 to float for DSP processing
    input.ToFloat(near_time_);

    // Get far-end reference from queue (what's being played through speaker)
    rt::DSPFrameBuffer far_end_frame;
    if (far_end_queue_->Pop(far_end_frame)) {
        far_end_frame.ToFloat(far_time_);

        // Phase 6: Capture far-end reference for recording
        if (far_end_writer_) {
            far_end_writer_->WriteSamples(far_end_frame.samples, rt::DSPFrameBuffer::kFrameSize);
        }
    } else {
        // No far-end available (queue empty) - use silence
        std::memset(far_time_, 0, sizeof(far_time_));
    }

    // Phase 4 M2: AEC processing (time domain)
    float error_time[rt::DSPFrameBuffer::kFrameSize];  // AEC output

    if (aec_enabled_) {
        // Run AEC in time domain (160 samples)
        // Use DTD state from previous frame to control adaptation
        bool is_double_talk = (previous_dtd_state_ == aec::DTD::DOUBLE_TALK);
        aec_->ProcessHop(near_time_, far_time_, error_time, is_double_talk);

        // Update ERLE metrics
        if (metrics_) {
            metrics_->UpdateERLE(aec_->GetERLE());
        }
    } else {
        // AEC disabled: pass-through
        std::memcpy(error_time, near_time_, rt::DSPFrameBuffer::kFrameSize * sizeof(float));
    }

    // STFT: Time domain → Frequency domain (separate instances to avoid state corruption)
    stft_error_->ProcessHop(error_time, error_spectrum_);
    stft_near_->ProcessHop(near_time_, near_spectrum_);
    stft_far_->ProcessHop(far_time_, far_spectrum_);

    // Phase 4 M2: DTD + RES processing (frequency domain)
    if (aec_enabled_) {
        // Run DTD
        aec::DTD::State dtd_state = dtd_->ProcessFrame(
            near_spectrum_, far_spectrum_, error_spectrum_);

        // Store DTD state for next frame's AEC adaptation
        previous_dtd_state_ = dtd_state;

        bool is_double_talk = (dtd_state == aec::DTD::DOUBLE_TALK);

        // Run RES on AEC error spectrum
        if (res_enabled_) { res_->ProcessFrame(error_spectrum_, far_spectrum_, is_double_talk);
        }

        // Update DTD metrics
        if (metrics_) {
            metrics_->UpdateDTDState(static_cast<int>(dtd_state));
        }
    }

#ifdef HAVE_TFLITE
    // Phase 5 M3: Denoiser processing (frequency domain)
    // Phase 7 T092: Denoiser decimation (process every Nth frame)    bool should_run_denoiser = denoiser_enabled_ && denoiser_ && denoiser_->IsReady() &&                               (frame_counter_ % denoiser_decimation_ == 0);    if (should_run_denoiser) {
        // Step 1: Compute magnitude spectrum from complex error_spectrum_
        // error_spectrum_ is [re0, im0, re1, im1, ..., re160, im160] (322 floats)
        // magnitude_[i] = sqrt(re[i]^2 + im[i]^2)
        float input_power = 0.0f;  // For SI-SDR computation
        for (int i = 0; i < 161; ++i) {
            float re = error_spectrum_[2 * i];
            float im = error_spectrum_[2 * i + 1];
            magnitude_[i] = std::sqrt(re * re + im * im);
            input_power += magnitude_[i] * magnitude_[i];
        }

        // Step 2: Run denoiser inference to get gain values
        if (denoiser_->ProcessSpectrum(magnitude_, gain_)) {
            // Step 3: Compute output power for SI-SDR (before applying gains)
            float output_power = 0.0f;
            for (int i = 0; i < 161; ++i) {
                float output_mag = magnitude_[i] * gain_[i];
                output_power += output_mag * output_mag;
            }

            // Step 4: Estimate SI-SDR delta (approximation)
            // SI-SDR delta ≈ 10 * log10(output_power / input_power)
            // Positive values indicate improvement (noise reduction)
            if (input_power > 1e-10f && output_power > 1e-10f) {
                float siSdr_db = 10.0f * std::log10(output_power / input_power);
                if (metrics_) {
                    metrics_->UpdateSISDR(siSdr_db);
                }
            }

            // Step 5: Apply gains to complex spectrum
            // output_spectrum[i] = input_spectrum[i] * gain[i]
            for (int i = 0; i < 161; ++i) {
                error_spectrum_[2 * i] *= gain_[i];      // Real part
                error_spectrum_[2 * i + 1] *= gain_[i];  // Imaginary part
            }
        } else {
            // Inference failed - pass through unmodified
            LOGE("Denoiser inference failed - passing through");
        }
    }
#endif

    // ISTFT: Frequency domain → Time domain
    istft_->ProcessHop(error_spectrum_, output_time_);

    // Convert float back to int16
    output.FromFloat(output_time_);

    // Copy timestamp and clipping flag
    output.timestamp = input.timestamp;
    output.is_clipping = input.is_clipping || output.is_clipping;

    // Phase 6: Capture enhanced output for recording (after all processing)
    if (enhanced_writer_) {
        enhanced_writer_->WriteSamples(output.samples, rt::DSPFrameBuffer::kFrameSize);
    }
}

}  // namespace pipeline
}  // namespace edgeclear
