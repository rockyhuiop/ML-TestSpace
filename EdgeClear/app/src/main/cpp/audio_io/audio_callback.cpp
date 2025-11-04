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
      frame_counter_(0),
      capture_resampler_buffer_size_(0),
      render_buffer_size_(0),
      render_buffer_offset_(0) {
}

void AudioCallback::OnCaptureData(const int16_t* data, int32_t numFrames) {
    // Accumulate samples until we have 160, then push a frame
    // AAudio typically delivers 96-sample bursts, so we need to buffer
    constexpr int32_t kFrameSize = 160;

    static int call_count = 0;
    call_count++;
    if (call_count <= 10 || call_count % 100 == 0) {
        LOGE("OnCaptureData called: frames=%d, buffer_size=%d, call#=%d",
             numFrames, capture_resampler_buffer_size_, call_count);
    }

    int32_t offset = 0;
    int32_t frames_pushed = 0;
    while (offset < numFrames) {
        // Calculate how many samples we can copy to buffer
        int32_t space_left = kFrameSize - capture_resampler_buffer_size_;
        int32_t to_copy = numFrames - offset;
        if (to_copy > space_left) {
            to_copy = space_left;
        }

        // Copy samples to accumulator buffer
        for (int32_t i = 0; i < to_copy; ++i) {
            capture_resampler_buffer_[capture_resampler_buffer_size_ + i] = data[offset + i];
        }
        capture_resampler_buffer_size_ += to_copy;
        offset += to_copy;

        // If we have a complete frame, push it
        if (capture_resampler_buffer_size_ >= kFrameSize) {
            rt::DSPFrameBuffer frame;

            // Copy accumulated samples to frame
            for (int32_t i = 0; i < kFrameSize; ++i) {
                frame.samples[i] = capture_resampler_buffer_[i];
            }

            // Set timestamp
            frame.timestamp = frame_counter_.fetch_add(kFrameSize,
                                                       std::memory_order_relaxed);

            // Detect clipping
            frame.is_clipping = false;
            for (int32_t i = 0; i < kFrameSize; ++i) {
                if (frame.samples[i] >= 16384 || frame.samples[i] <= -16384) {
                    frame.is_clipping = true;
                    break;
                }
            }

            // Push to input queue
            if (!input_queue_->Push(frame)) {
                // Queue overflow - drop frame and increment XRun counter
                perf_counters_->IncrementXRun();
                LOGE("Capture: Input queue OVERFLOW - frame dropped");
            } else {
                frames_pushed++;
            }

            // Reset buffer
            capture_resampler_buffer_size_ = 0;
        }
    }

    if (call_count <= 10 || frames_pushed > 0) {
        LOGE("OnCaptureData: pushed %d frames, buffer_size now=%d",
             frames_pushed, capture_resampler_buffer_size_);
    }
}

void AudioCallback::OnRenderData(int16_t* data, int32_t numFrames) {
    // Output frames at 48kHz, handling partial frame buffering
    // AAudio typically requests 96-sample bursts, but we process 160-sample frames
    // So we need to buffer partial frames

    int32_t written = 0;

    while (written < numFrames) {
        // First, try to use any leftover samples from previous frame
        if (render_buffer_size_ > 0) {
            int32_t available = render_buffer_size_ - render_buffer_offset_;
            int32_t to_copy = (numFrames - written < available) ? (numFrames - written) : available;

            for (int32_t i = 0; i < to_copy; ++i) {
                data[written + i] = render_buffer_[render_buffer_offset_ + i];
            }

            written += to_copy;
            render_buffer_offset_ += to_copy;

            // If we've consumed the entire buffer, mark it as empty
            if (render_buffer_offset_ >= render_buffer_size_) {
                render_buffer_size_ = 0;
                render_buffer_offset_ = 0;
            }

            continue;
        }

        // No buffered data - pop a new frame from output queue
        rt::DSPFrameBuffer frame;
        if (output_queue_->Pop(frame)) {
            // Push to far-end queue for AEC reference
            if (!far_end_queue_->Push(frame)) {
                // Far-end queue overflow (should be rare)
                LOGE("Render: Far-end queue overflow");
            }

            // Copy frame to render buffer
            for (int32_t i = 0; i < 160; ++i) {
                render_buffer_[i] = frame.samples[i];
            }
            render_buffer_size_ = 160;
            render_buffer_offset_ = 0;

            // Continue to next iteration to copy from buffer
        } else {
            // Queue underflow - fill rest with silence and increment XRun
            std::memset(data + written, 0, (numFrames - written) * sizeof(int16_t));
            perf_counters_->IncrementXRun();
            LOGE("Render: Output queue underflow - filling with silence (written=%d, needed=%d)",
                 written, numFrames);
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
