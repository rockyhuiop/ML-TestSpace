#pragma once

#include "../rt/ring_buffer.h"
#include "../rt/frame_buffer.h"
#include "../rt/perf_counters.h"
#include "simple_resampler.h"
#include <atomic>

namespace edgeclear {
namespace audio_io {

/**
 * AudioCallback: Real-time audio callback handler
 *
 * Bridges AAudio callbacks to SPSC queues for DSP worker thread.
 *
 * Constitution requirements:
 * - Zero allocations in RT callback
 * - No mutexes, only atomic operations
 * - Minimal processing (<1 ms target)
 *
 * Architecture:
 * - Capture callback: Push frames to input queue
 * - Render callback: Pop frames from output queue
 * - XRun tracking on queue overflow/underflow
 */
class AudioCallback {
public:
    AudioCallback(rt::SPSCRingBuffer<rt::DSPFrameBuffer>* input_queue,
                  rt::SPSCRingBuffer<rt::DSPFrameBuffer>* output_queue,
                  rt::SPSCRingBuffer<rt::DSPFrameBuffer>* far_end_queue,
                  rt::PerfCounters* perf_counters);

    ~AudioCallback() = default;

    // Non-copyable
    AudioCallback(const AudioCallback&) = delete;
    AudioCallback& operator=(const AudioCallback&) = delete;

    /**
     * Capture callback: Process incoming audio from microphone.
     *
     * Called by AAudio capture stream on RT thread.
     * MUST complete in <1 ms to avoid XRuns.
     *
     * @param data Input audio samples (int16, mono)
     * @param numFrames Number of frames (typically 96-480)
     */
    void OnCaptureData(const int16_t* data, int32_t numFrames);

    /**
     * Render callback: Provide outgoing audio to speaker/headphones.
     *
     * Called by AAudio render stream on RT thread.
     * MUST complete in <1 ms to avoid XRuns.
     *
     * @param data Output audio buffer to fill (int16, mono)
     * @param numFrames Number of frames (typically 96-480)
     */
    void OnRenderData(int16_t* data, int32_t numFrames);

    /**
     * Enable denormal flush (FTZ/DAZ) for RT thread.
     * Call once during thread initialization.
     */
    static void EnableDenormalFlush();

private:
    /**
     * Helper function to decimate one frame from input.
     * Returns true if a complete frame was produced.
     *
     * @param input Input samples @ 48kHz
     * @param input_count Number of input samples available
     * @param output Output buffer @ 16kHz (160 samples)
     * @param output_count Number of samples written to output
     * @param consumed Number of input samples consumed
     * @return true if a complete frame (160 samples) was produced
     */
    bool DecimateOnce(const int16_t* input, int32_t input_count,
                      int16_t* output, int32_t* output_count,
                      int32_t* consumed);

    rt::SPSCRingBuffer<rt::DSPFrameBuffer>* input_queue_;
    rt::SPSCRingBuffer<rt::DSPFrameBuffer>* output_queue_;
    rt::SPSCRingBuffer<rt::DSPFrameBuffer>* far_end_queue_;  // AEC reference
    rt::PerfCounters* perf_counters_;

    // Frame timestamp counter (for alignment validation)
    std::atomic<uint64_t> frame_counter_;

    // Resampler with accumulation buffering
    SimpleResampler capture_resampler_;
    SimpleResampler render_resampler_;

    // Manual buffer for capture (accumulates AAudio bursts into 160-sample frames)
    int16_t capture_resampler_buffer_[SimpleResampler::kInputFramesPerHop] = {0};
    int32_t capture_resampler_buffer_size_ = 0;

    // Manual buffer for render (holds leftover samples from 160-sample frames)
    int16_t render_buffer_[160] = {0};
    int32_t render_buffer_size_ = 0;
    int32_t render_buffer_offset_ = 0;
};

}  // namespace audio_io
}  // namespace edgeclear
