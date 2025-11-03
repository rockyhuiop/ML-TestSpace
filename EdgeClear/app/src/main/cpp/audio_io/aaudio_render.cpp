#include "aaudio_render.h"
#include <android/log.h>
#include <cstring>

#define LOG_TAG "EdgeClear-Render"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace edgeclear {
namespace audio_io {

AAudioRender::AAudioRender()
    : stream_(nullptr),
      is_running_(false) {
}

AAudioRender::~AAudioRender() {
    Stop();
    if (stream_) {
        AAudioStream_close(stream_);
        stream_ = nullptr;
    }
}

bool AAudioRender::Initialize(int32_t sampleRate, int32_t framesPerBurst,
                               DataCallback callback) {
    data_callback_ = callback;

    AAudioStreamBuilder* builder;
    aaudio_result_t result = AAudio_createStreamBuilder(&builder);
    if (result != AAUDIO_OK) {
        LOGE("Failed to create stream builder: %s", AAudio_convertResultToText(result));
        return false;
    }

    // Configure render stream
    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setSampleRate(builder, sampleRate);
    AAudioStreamBuilder_setChannelCount(builder, 1);  // Mono
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);

    // Low-latency performance mode (Constitution requirement)
    AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);

    // Try exclusive sharing mode (fallback to shared if unavailable)
    AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_EXCLUSIVE);

    // Set frames per burst (buffer size)
    AAudioStreamBuilder_setFramesPerBurst(builder, framesPerBurst);

    // Register callbacks
    AAudioStreamBuilder_setDataCallback(builder, DataCallbackStatic, this);
    AAudioStreamBuilder_setErrorCallback(builder, ErrorCallbackStatic, this);

    // Open stream
    result = AAudioStreamBuilder_openStream(builder, &stream_);
    AAudioStreamBuilder_delete(builder);

    if (result != AAUDIO_OK) {
        LOGE("Failed to open render stream: %s", AAudio_convertResultToText(result));
        return false;
    }

    // Log stream configuration
    int32_t actualSampleRate = AAudioStream_getSampleRate(stream_);
    int32_t actualFramesPerBurst = AAudioStream_getFramesPerBurst(stream_);
    aaudio_sharing_mode_t actualSharingMode = AAudioStream_getSharingMode(stream_);

    LOGI("Render stream opened:");
    LOGI("  Sample rate: %d Hz (requested %d)", actualSampleRate, sampleRate);
    LOGI("  Frames per burst: %d (requested %d)", actualFramesPerBurst, framesPerBurst);
    LOGI("  Sharing mode: %s",
         actualSharingMode == AAUDIO_SHARING_MODE_EXCLUSIVE ? "Exclusive" : "Shared");

    return true;
}

bool AAudioRender::Start() {
    if (!stream_) {
        LOGE("Cannot start: stream not initialized");
        return false;
    }

    aaudio_result_t result = AAudioStream_requestStart(stream_);
    if (result != AAUDIO_OK) {
        LOGE("Failed to start render stream: %s", AAudio_convertResultToText(result));
        return false;
    }

    is_running_.store(true, std::memory_order_release);
    LOGI("Render stream started");
    return true;
}

bool AAudioRender::Stop() {
    if (!stream_) {
        return true;
    }

    is_running_.store(false, std::memory_order_release);

    aaudio_result_t result = AAudioStream_requestStop(stream_);
    if (result != AAUDIO_OK) {
        LOGE("Failed to stop render stream: %s", AAudio_convertResultToText(result));
        return false;
    }

    LOGI("Render stream stopped");
    return true;
}

int32_t AAudioRender::GetFramesPerBurst() const {
    return stream_ ? AAudioStream_getFramesPerBurst(stream_) : 0;
}

// Static callback adapter
aaudio_data_callback_result_t AAudioRender::DataCallbackStatic(
        AAudioStream* stream,
        void* userData,
        void* audioData,
        int32_t numFrames) {

    auto* render = static_cast<AAudioRender*>(userData);

    if (render && render->data_callback_) {
        // Invoke user callback to fill audio buffer
        render->data_callback_(static_cast<int16_t*>(audioData), numFrames);
    } else {
        // No callback - output silence
        std::memset(audioData, 0, numFrames * sizeof(int16_t));
    }

    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

void AAudioRender::ErrorCallbackStatic(
        AAudioStream* stream,
        void* userData,
        aaudio_result_t error) {

    auto* render = static_cast<AAudioRender*>(userData);

    LOGE("AAudio error callback: %s", AAudio_convertResultToText(error));

    // Handle disconnection (e.g., headphones unplugged)
    if (error == AAUDIO_ERROR_DISCONNECTED) {
        LOGE("Audio device disconnected - stream needs to be recreated");
        render->is_running_.store(false, std::memory_order_release);
    }
}

}  // namespace audio_io
}  // namespace edgeclear
