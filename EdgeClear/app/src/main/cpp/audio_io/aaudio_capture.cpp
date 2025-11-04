#include "aaudio_capture.h"
#include <android/log.h>

#define LOG_TAG "EdgeClear-Capture"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace edgeclear {
namespace audio_io {

AAudioCapture::AAudioCapture()
    : stream_(nullptr),
      is_running_(false) {
}

AAudioCapture::~AAudioCapture() {
    Stop();
    if (stream_) {
        AAudioStream_close(stream_);
        stream_ = nullptr;
    }
}

bool AAudioCapture::Initialize(int32_t sampleRate, int32_t framesPerBurst,
                                DataCallback callback) {
    data_callback_ = callback;

    AAudioStreamBuilder* builder;
    aaudio_result_t result = AAudio_createStreamBuilder(&builder);
    if (result != AAUDIO_OK) {
        LOGE("Failed to create stream builder: %s", AAudio_convertResultToText(result));
        return false;
    }

    // Configure capture stream
    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_INPUT);
    AAudioStreamBuilder_setSampleRate(builder, sampleRate);
    AAudioStreamBuilder_setChannelCount(builder, 1);  // Mono
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);

    // Low-latency performance mode (Constitution requirement)
    AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);

    // Try exclusive sharing mode (fallback to shared if unavailable)
    AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_EXCLUSIVE);

    // Set frames per burst (buffer size) - Note: setFramesPerBurst not available in older NDK
    // AAudioStreamBuilder_setFramesPerBurst(builder, framesPerBurst);
    (void)framesPerBurst;  // Suppress unused parameter warning

    // Register callbacks
    AAudioStreamBuilder_setDataCallback(builder, DataCallbackStatic, this);
    AAudioStreamBuilder_setErrorCallback(builder, ErrorCallbackStatic, this);

    // Open stream
    result = AAudioStreamBuilder_openStream(builder, &stream_);
    AAudioStreamBuilder_delete(builder);

    if (result != AAUDIO_OK) {
        LOGE("Failed to open capture stream: %s", AAudio_convertResultToText(result));
        return false;
    }

    // Log stream configuration
    int32_t actualSampleRate = AAudioStream_getSampleRate(stream_);
    int32_t actualFramesPerBurst = AAudioStream_getFramesPerBurst(stream_);
    aaudio_sharing_mode_t actualSharingMode = AAudioStream_getSharingMode(stream_);

    LOGI("Capture stream opened:");
    LOGI("  Sample rate: %d Hz (requested %d)", actualSampleRate, sampleRate);
    LOGI("  Frames per burst: %d (requested %d)", actualFramesPerBurst, framesPerBurst);
    LOGI("  Sharing mode: %s",
         actualSharingMode == AAUDIO_SHARING_MODE_EXCLUSIVE ? "Exclusive" : "Shared");

    return true;
}

bool AAudioCapture::Start() {
    if (!stream_) {
        LOGE("Cannot start: stream not initialized");
        return false;
    }

    aaudio_result_t result = AAudioStream_requestStart(stream_);
    if (result != AAUDIO_OK) {
        LOGE("Failed to start capture stream: %s", AAudio_convertResultToText(result));
        return false;
    }

    is_running_.store(true, std::memory_order_release);
    LOGI("Capture stream started");
    return true;
}

bool AAudioCapture::Stop() {
    if (!stream_) {
        return true;
    }

    is_running_.store(false, std::memory_order_release);

    aaudio_result_t result = AAudioStream_requestStop(stream_);
    if (result != AAUDIO_OK) {
        LOGE("Failed to stop capture stream: %s", AAudio_convertResultToText(result));
        return false;
    }

    LOGI("Capture stream stopped");
    return true;
}

int32_t AAudioCapture::GetFramesPerBurst() const {
    return stream_ ? AAudioStream_getFramesPerBurst(stream_) : 0;
}

// Static callback adapter
aaudio_data_callback_result_t AAudioCapture::DataCallbackStatic(
        AAudioStream* stream,
        void* userData,
        void* audioData,
        int32_t numFrames) {
    (void)stream;  // Suppress unused parameter warning

    auto* capture = static_cast<AAudioCapture*>(userData);

    if (capture && capture->data_callback_) {
        // Invoke user callback with audio data
        capture->data_callback_(static_cast<const int16_t*>(audioData), numFrames);
    }

    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

void AAudioCapture::ErrorCallbackStatic(
        AAudioStream* stream,
        void* userData,
        aaudio_result_t error) {
    (void)stream;  // Suppress unused parameter warning

    auto* capture = static_cast<AAudioCapture*>(userData);

    LOGE("AAudio error callback: %s", AAudio_convertResultToText(error));

    // Handle disconnection (e.g., headphones unplugged)
    if (error == AAUDIO_ERROR_DISCONNECTED) {
        LOGE("Audio device disconnected - stream needs to be recreated");
        capture->is_running_.store(false, std::memory_order_release);
    }
}

}  // namespace audio_io
}  // namespace edgeclear
