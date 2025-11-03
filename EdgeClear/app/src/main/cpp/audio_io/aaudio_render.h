#pragma once

#include <aaudio/AAudio.h>
#include <atomic>
#include <functional>

namespace edgeclear {
namespace audio_io {

/**
 * AAudioRender: Low-latency audio output stream
 *
 * Wraps AAudio API for speaker/headphone output with:
 * - Low-latency performance mode
 * - Exclusive sharing mode (if available)
 * - 48 kHz sample rate
 * - Callback-driven processing
 *
 * Constitution requirements:
 * - Zero allocations in audio callback
 * - <10 ms buffer latency
 */
class AAudioRender {
public:
    using DataCallback = std::function<void(int16_t* data, int32_t numFrames)>;

    AAudioRender();
    ~AAudioRender();

    // Non-copyable
    AAudioRender(const AAudioRender&) = delete;
    AAudioRender& operator=(const AAudioRender&) = delete;

    /**
     * Initialize render stream.
     *
     * @param sampleRate Sample rate in Hz (48000)
     * @param framesPerBurst Frames per callback
     * @param callback Data callback invoked by AAudio thread
     * @return true on success
     */
    bool Initialize(int32_t sampleRate, int32_t framesPerBurst, DataCallback callback);

    /**
     * Start render stream.
     */
    bool Start();

    /**
     * Stop render stream.
     */
    bool Stop();

    /**
     * Get actual frames per burst (may differ from requested).
     */
    int32_t GetFramesPerBurst() const;

    /**
     * Get stream state.
     */
    bool IsRunning() const { return is_running_.load(std::memory_order_acquire); }

private:
    AAudioStream* stream_;
    DataCallback data_callback_;
    std::atomic<bool> is_running_;

    // Static callback adapter (AAudio requires C-style callback)
    static aaudio_data_callback_result_t DataCallbackStatic(
        AAudioStream* stream,
        void* userData,
        void* audioData,
        int32_t numFrames);

    static void ErrorCallbackStatic(
        AAudioStream* stream,
        void* userData,
        aaudio_result_t error);
};

}  // namespace audio_io
}  // namespace edgeclear
