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
    rt::SPSCRingBuffer<rt::DSPFrameBuffer>* input_queue_;
    rt::SPSCRingBuffer<rt::DSPFrameBuffer>* output_queue_;
    rt::SPSCRingBuffer<rt::DSPFrameBuffer>* far_end_queue_;  // AEC reference
    rt::PerfCounters* perf_counters_;

    // Frame timestamp counter (for alignment validation)
    std::atomic<uint64_t> frame_counter_;

    // Resampler with accumulation buffering
    SimpleResampler capture_resampler_;
    SimpleResampler render_resampler_;
};

}  // namespace audio_io
}  // namespace edgeclear
