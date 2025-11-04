#include "audio_callback.h"
#include <android/log.h>
#include <cstring>
#ifdef __ARM_NEON
#include <arm_neon.h>  // For FTZ/DAZ on ARM
#endif

#define LOG_TAG "EdgeClear-Callback"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace edgeclear {
namespace audio_io {

AudioCallback::AudioCallback(
        rt::SPSCRingBuffer<rt::DSPFrameBuffer>* input_queue,
        rt::SPSCRingBuffer<rt::DSPFrameBuffer>* output_queue,
        rt::SPSCRingBuffer<rt::DSPFrameBuffer>* far_end_queue,
        rt::PerfCounters* perf_counters)
    : input_queue_(input_queue),
      output_queue_(output_queue),
      far_end_queue_(far_end_queue),
      perf_counters_(perf_counters),
      frame_counter_(0) {
}

void AudioCallback::OnCaptureData(const int16_t* data, int32_t numFrames) {
    // Decimate 48→16 kHz with accumulation buffering
    // Accumulates until 480 samples @ 48 kHz are available,
    // then produces 160 samples @ 16 kHz

    static int callback_count = 0;
    if (++callback_count % 100 == 0) {  // Log every 100th callback (~1 second)
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
            "OnCaptureData called: numFrames=%d, callback_count=%d", numFrames, callback_count);
    }

    rt::DSPFrameBuffer frame;
    int32_t output_count = 0;

    // Try to decimate (may return false if not enough samples yet)
    if (capture_resampler_.Decimate(data, numFrames, frame.samples, &output_count)) {
        // Got a full hop (160 samples @ 16 kHz)

        // Set timestamp
        frame.timestamp = frame_counter_.fetch_add(rt::DSPFrameBuffer::kFrameSize,
                                                   std::memory_order_relaxed);

        // Detect clipping
        frame.is_clipping = false;
        for (int32_t i = 0; i < rt::DSPFrameBuffer::kFrameSize; ++i) {
            if (frame.samples[i] >= 16384 || frame.samples[i] <= -16384) {
                frame.is_clipping = true;
                break;
            }
        }

        // Push to input queue
        if (!input_queue_->Push(frame)) {
            // Queue overflow - drop frame and increment XRun counter
            perf_counters_->IncrementXRun();
            __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                "Input queue overflow - capture producing too fast");
        } else {
            static int push_count = 0;
            if (++push_count % 50 == 0) {  // Log every 50th push (~0.5 seconds)
                __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                    "Pushed frame to input queue: count=%d", push_count);
            }
        }
    }
    // else: Not enough samples yet, will accumulate on next callback
}

void AudioCallback::OnRenderData(int16_t* data, int32_t numFrames) {
    // We need to fill numFrames @ 48 kHz
    // Each DSP hop produces 160 samples @ 16 kHz = 480 samples @ 48 kHz

    static int callback_count = 0;
    if (++callback_count % 100 == 0) {  // Log every 100th callback (~1 second)
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
            "OnRenderData called: numFrames=%d, callback_count=%d", numFrames, callback_count);
    }

    int32_t written = 0;

    while (written < numFrames) {
        rt::DSPFrameBuffer frame;

        // Pop from output queue
        if (output_queue_->Pop(frame)) {
            // Push to far-end queue for AEC reference (before upsampling)
            // This captures what actually gets played through the speaker
            if (!far_end_queue_->Push(frame)) {
                // Far-end queue overflow (should be rare)
                // This is less critical than capture XRun, so don't increment counter
            }

            // Interpolate 16→48 kHz
            int16_t interpolated[SimpleResampler::kInputFramesPerHop];
            int32_t interp_count = 0;

            render_resampler_.Interpolate(frame.samples, interpolated, &interp_count);

            // Copy to output (up to numFrames)
            int32_t to_copy = (written + interp_count <= numFrames) ?
                             interp_count : numFrames - written;
            std::memcpy(data + written, interpolated, to_copy * sizeof(int16_t));
            written += to_copy;
        } else {
            // Queue underflow - fill rest with silence and increment XRun
            std::memset(data + written, 0, (numFrames - written) * sizeof(int16_t));
            perf_counters_->IncrementXRun();
            break;
        }
    }
}

void AudioCallback::EnableDenormalFlush() {
#ifdef __ARM_NEON
    // Enable FTZ (Flush-To-Zero) on ARM NEON
    // This prevents denormal numbers from degrading performance
#ifdef __aarch64__
    // ARM64: use FPCR register
    uint64_t fpcr;
    __asm__ __volatile__("mrs %0, fpcr" : "=r"(fpcr));
    fpcr |= (1 << 24);  // FZ bit: flush denormals to zero
    __asm__ __volatile__("msr fpcr, %0" : : "r"(fpcr));
#else
    // ARM32: use FPSCR register
    uint32_t fpscr;
    __asm__ __volatile__("vmrs %0, fpscr" : "=r"(fpscr));
    fpscr |= (1 << 24);  // FZ bit: flush denormals to zero
    __asm__ __volatile__("vmsr fpscr, %0" : : "r"(fpscr));
#endif
#endif
}

}  // namespace audio_io
}  // namespace edgeclear
