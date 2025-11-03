#include "session_manager.h"
#include "../audio_io/latency_probe.h"
#include <android/log.h>

#define LOG_TAG "EdgeClear-Session"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace edgeclear {
namespace pipeline {

SessionManager::SessionManager()
    : is_initialized_(false),
      is_active_(false),
      sample_rate_(0),
      hop_size_(0) {
}

SessionManager::~SessionManager() {
    Destroy();
}

bool SessionManager::Initialize(int32_t sampleRate, int32_t hopSize,
                                 const std::string& preset,
                                 AAssetManager* assetManager) {
    if (is_initialized_) {
        LOGE("Session already initialized");
        return false;
    }

    LOGI("Initializing session: sampleRate=%d, hopSize=%d, preset=%s",
         sampleRate, hopSize, preset.c_str());

    sample_rate_ = sampleRate;
    hop_size_ = hopSize;
    preset_ = preset;

    // Create performance counters
    perf_counters_ = std::make_unique<rt::PerfCounters>();

    // Create metrics accumulator
    metrics_ = std::make_unique<MetricsAccumulator>();
    metrics_->SetPerfCounters(perf_counters_.get());

    // Determine queue size based on preset
    // Quality/Battery saver: 4 hops buffering (40 ms)
    // Low-latency: 2 hops buffering (20 ms)
    const size_t queue_capacity = (preset == "Low-latency") ? 2 : 4;

    // Create SPSC queues
    input_queue_ = std::make_unique<rt::SPSCRingBuffer<rt::DSPFrameBuffer>>(queue_capacity);
    output_queue_ = std::make_unique<rt::SPSCRingBuffer<rt::DSPFrameBuffer>>(queue_capacity);
    far_end_queue_ = std::make_unique<rt::SPSCRingBuffer<rt::DSPFrameBuffer>>(queue_capacity);

    LOGI("Created SPSC queues with capacity=%zu hops", queue_capacity);

    // Create audio callback handler (with far-end queue for AEC reference)
    audio_callback_ = std::make_unique<audio_io::AudioCallback>(
        input_queue_.get(),
        output_queue_.get(),
        far_end_queue_.get(),
        perf_counters_.get()
    );

    // Phase 5 M3: Try to initialize denoiser model (non-fatal if fails)
    if (assetManager) {
        InitializeDenoiser(assetManager);
    }

    // Create DSP worker (pass metrics for AEC/DTD updates, far-end queue, and denoiser model)
    dsp_worker_ = std::make_unique<DSPWorker>(
        input_queue_.get(),
        output_queue_.get(),
        far_end_queue_.get(),
        perf_counters_.get(),
        metrics_.get(),
        denoiser_model_.get()  // May be nullptr if model load failed
    );

    // Create AAudio streams
    capture_ = std::make_unique<audio_io::AAudioCapture>();
    render_ = std::make_unique<audio_io::AAudioRender>();

    // Initialize capture stream
    // Use device's native frames per burst (typically 96-240)
    const int32_t framesPerBurst = 192;  // Common value, device may override

    auto captureCallback = [this](const int16_t* data, int32_t numFrames) {
        audio_callback_->OnCaptureData(data, numFrames);
    };

    if (!capture_->Initialize(sample_rate_, framesPerBurst, captureCallback)) {
        LOGE("Failed to initialize capture stream");
        return false;
    }

    // Initialize render stream
    auto renderCallback = [this](int16_t* data, int32_t numFrames) {
        audio_callback_->OnRenderData(data, numFrames);
    };

    if (!render_->Initialize(sample_rate_, framesPerBurst, renderCallback)) {
        LOGE("Failed to initialize render stream");
        return false;
    }

    LOGI("AAudio streams initialized successfully");

    // Enable denormal flush for RT threads
    audio_io::AudioCallback::EnableDenormalFlush();

    // Estimate latency based on configuration
    const float estimated_latency = audio_io::LatencyProbe::MeasureLatency(
        sample_rate_, queue_capacity);
    metrics_->SetLatency(estimated_latency);
    LOGI("Estimated latency: %.1f ms", estimated_latency);

    is_initialized_ = true;
    LOGI("Session initialized successfully");
    return true;
}

bool SessionManager::Start() {
    if (!is_initialized_) {
        LOGE("Cannot start: session not initialized");
        return false;
    }

    if (is_active_) {
        LOGE("Session already active");
        return false;
    }

    LOGI("Starting audio processing session");

    // Start DSP worker thread first
    if (!dsp_worker_->Start()) {
        LOGE("Failed to start DSP worker");
        return false;
    }

    // Start AAudio streams
    if (!capture_->Start()) {
        LOGE("Failed to start capture stream");
        dsp_worker_->Stop();
        return false;
    }

    if (!render_->Start()) {
        LOGE("Failed to start render stream");
        capture_->Stop();
        dsp_worker_->Stop();
        return false;
    }

    is_active_ = true;
    LOGI("Audio processing session started successfully");
    return true;
}

void SessionManager::Stop() {
    if (!is_active_) {
        return;
    }

    LOGI("Stopping audio processing session");

    // Stop AAudio streams first (stops RT callbacks)
    if (capture_) {
        capture_->Stop();
    }

    if (render_) {
        render_->Stop();
    }

    // Stop DSP worker thread
    if (dsp_worker_) {
        dsp_worker_->Stop();
    }

    is_active_ = false;
    LOGI("Audio processing session stopped");
}

void SessionManager::Destroy() {
    Stop();

    LOGI("Destroying session");

    // Destroy in reverse order of creation
    render_.reset();
    capture_.reset();
    dsp_worker_.reset();
    audio_callback_.reset();
    far_end_queue_.reset();
    output_queue_.reset();
    input_queue_.reset();
    metrics_.reset();
    perf_counters_.reset();

    is_initialized_ = false;
    LOGI("Session destroyed");
}

bool SessionManager::IsActive() const {
    return is_active_;
}

bool SessionManager::InitializeDenoiser(AAssetManager* assetManager) {
    if (!assetManager) {
        LOGI("No AssetManager provided - denoiser disabled");
        return false;
    }

    LOGI("Initializing denoiser model from assets");

    // Read model manifest to get expected SHA-256
    AAsset* manifest_asset = AAssetManager_open(assetManager,
        "models/model_manifest.json", AASSET_MODE_BUFFER);
    if (!manifest_asset) {
        LOGE("Failed to open model_manifest.json from assets");
        LOGI("This is expected if model not yet trained");
        return false;
    }

    const char* manifest_data = static_cast<const char*>(AAsset_getBuffer(manifest_asset));
    off_t manifest_len = AAsset_getLength(manifest_asset);
    std::string manifest_json(manifest_data, manifest_len);
    AAsset_close(manifest_asset);

    // Parse SHA-256 from JSON (simple string search, not full JSON parser)
    std::string expected_sha256;
    size_t sha_pos = manifest_json.find("\"sha256\"");
    if (sha_pos != std::string::npos) {
        size_t colon_pos = manifest_json.find(":", sha_pos);
        size_t quote1 = manifest_json.find("\"", colon_pos);
        size_t quote2 = manifest_json.find("\"", quote1 + 1);
        if (quote1 != std::string::npos && quote2 != std::string::npos) {
            expected_sha256 = manifest_json.substr(quote1 + 1, quote2 - quote1 - 1);
        }
    }

    if (expected_sha256.empty() ||
        expected_sha256.find("placeholder") != std::string::npos) {
        LOGE("Invalid or placeholder SHA-256 in manifest");
        return false;
    }

    // Read model file from assets
    AAsset* model_asset = AAssetManager_open(assetManager,
        "models/denoiser_tcn_int8.tflite", AASSET_MODE_BUFFER);
    if (!model_asset) {
        LOGE("Failed to open denoiser model from assets");
        LOGI("See assets/models/README.md for model training instructions");
        return false;
    }

    const void* model_data = AAsset_getBuffer(model_asset);
    off_t model_len = AAsset_getLength(model_asset);

    // Write to temp file (TFLite BuildFromFile needs file path)
    std::string temp_path = "/data/local/tmp/denoiser_tcn_int8.tflite";
    FILE* temp_file = fopen(temp_path.c_str(), "wb");
    if (!temp_file) {
        LOGE("Failed to create temp file for model");
        AAsset_close(model_asset);
        return false;
    }

    fwrite(model_data, 1, model_len, temp_file);
    fclose(temp_file);
    AAsset_close(model_asset);

    // Create model state
    denoiser_model_ = std::make_unique<ml::DenoiserModelState>();

    // Load model with SHA-256 verification
    if (!denoiser_model_->LoadModel(temp_path, expected_sha256)) {
        LOGE("Failed to load denoiser model - SHA-256 mismatch or load error");
        denoiser_model_.reset();
        return false;
    }

    // Initialize interpreter with delegate selection
    if (!denoiser_model_->InitializeInterpreter()) {
        LOGE("Failed to initialize denoiser interpreter");
        denoiser_model_.reset();
        return false;
    }

    LOGI("Denoiser model loaded successfully");
    LOGI("  Delegate: %s", denoiser_model_->GetDelegateNameString());
    LOGI("  Input quantization: scale=%.6f, zero_point=%d",
         denoiser_model_->GetInputScale(), denoiser_model_->GetInputZeroPoint());
    LOGI("  Output quantization: scale=%.6f, zero_point=%d",
         denoiser_model_->GetOutputScale(), denoiser_model_->GetOutputZeroPoint());

    return true;
}

}  // namespace pipeline
}  // namespace edgeclear
