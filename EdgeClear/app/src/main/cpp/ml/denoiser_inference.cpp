#include "denoiser_inference.h"
#include <android/log.h>
#include <chrono>
#include <cmath>
#include <algorithm>

#define LOG_TAG "EdgeClear-Denoiser"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace edgeclear {
namespace ml {

DenoiserInference::DenoiserInference()
    : model_state_(nullptr),
      is_initialized_(false),
      last_inference_time_us_(0) {
    // Zero buffers
    std::memset(input_quantized_, 0, sizeof(input_quantized_));
    std::memset(output_quantized_, 0, sizeof(output_quantized_));
}

bool DenoiserInference::Initialize(DenoiserModelState* model_state) {
    if (!model_state || !model_state->IsReady()) {
        LOGE("Cannot initialize - invalid or unready model state");
        return false;
    }

    model_state_ = model_state;
    is_initialized_ = true;

    LOGI("Denoiser inference initialized (delegate: %s)",
         model_state_->GetDelegateNameString());
    return true;
}

bool DenoiserInference::ProcessSpectrum(const float* magnitude, float* gain) {
    if (!is_initialized_) {
        LOGE("ProcessSpectrum called before Initialize()");
        return false;
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    // Step 1: Normalize magnitude to model's expected range
    // Model was trained on log-magnitude spectra normalized to [-1, 1]
    float normalized[DenoiserModelState::kInputSize];
    NormalizeMagnitude(magnitude, normalized, DenoiserModelState::kInputSize);

    // Step 2: Quantize to INT8
    QuantizeInput(normalized, input_quantized_,
                  model_state_->GetInputScale(),
                  model_state_->GetInputZeroPoint(),
                  DenoiserModelState::kInputSize);

    // Step 3: Copy to TFLite input tensor
    tflite::Interpreter* interpreter = model_state_->GetInterpreter();
    int input_idx = model_state_->GetInputTensorIndex();
    TfLiteTensor* input_tensor = interpreter->tensor(input_idx);
    std::memcpy(input_tensor->data.int8, input_quantized_,
                DenoiserModelState::kInputSize * sizeof(int8_t));

    // Step 4: Run inference
    if (interpreter->Invoke() != kTfLiteOk) {
        LOGE("Inference failed");
        return false;
    }

    // Step 5: Copy from TFLite output tensor
    int output_idx = model_state_->GetOutputTensorIndex();
    TfLiteTensor* output_tensor = interpreter->tensor(output_idx);
    std::memcpy(output_quantized_, output_tensor->data.int8,
                DenoiserModelState::kOutputSize * sizeof(int8_t));

    // Step 6: Dequantize to float32
    DequantizeOutput(output_quantized_, gain,
                     model_state_->GetOutputScale(),
                     model_state_->GetOutputZeroPoint(),
                     DenoiserModelState::kOutputSize);

    // Step 7: Clamp gains to [0, 1] for safety
    for (int i = 0; i < DenoiserModelState::kOutputSize; ++i) {
        gain[i] = std::clamp(gain[i], 0.0f, 1.0f);
    }

    // Measure inference time
    auto end_time = std::chrono::high_resolution_clock::now();
    last_inference_time_us_ = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time).count();

    return true;
}

void DenoiserInference::QuantizeInput(const float* input, int8_t* output,
                                       float scale, int32_t zero_point, int size) {
    // Formula: q = clip(round(f / scale) + zero_point, -128, 127)
    for (int i = 0; i < size; ++i) {
        float scaled = input[i] / scale + static_cast<float>(zero_point);
        int32_t quantized = static_cast<int32_t>(std::round(scaled));
        output[i] = static_cast<int8_t>(std::clamp(quantized, -128, 127));
    }
}

void DenoiserInference::DequantizeOutput(const int8_t* input, float* output,
                                          float scale, int32_t zero_point, int size) {
    // Formula: f = (q - zero_point) * scale
    for (int i = 0; i < size; ++i) {
        output[i] = (static_cast<float>(input[i]) - static_cast<float>(zero_point)) * scale;
    }
}

void DenoiserInference::NormalizeMagnitude(const float* magnitude, float* normalized, int size) {
    // Denoiser was trained on log-magnitude spectra normalized to approximately [-1, 1]
    // Typical magnitude range for int16 audio: [0, ~32767]
    // Strategy:
    //   1. Compute log-magnitude: log10(max(mag, 1e-6))
    //   2. Normalize to [-1, 1]: (log_mag - log_min) / (log_max - log_min) * 2 - 1
    //
    // Expected ranges:
    //   - log10(1e-6) = -6.0 (silence/noise floor)
    //   - log10(32767) = 4.5 (full scale)
    //   - Normalize: [-6, 4.5] → [-1, 1]

    constexpr float kLogMin = -6.0f;  // log10(1e-6) - silence floor
    constexpr float kLogMax = 4.5f;   // log10(32767) - full scale
    constexpr float kLogRange = kLogMax - kLogMin;
    constexpr float kEpsilon = 1e-6f;  // Avoid log(0)

    for (int i = 0; i < size; ++i) {
        // Compute log-magnitude
        float log_mag = std::log10(std::max(magnitude[i], kEpsilon));

        // Normalize to [-1, 1]
        float normalized_val = (log_mag - kLogMin) / kLogRange * 2.0f - 1.0f;

        // Clamp to [-1, 1] for safety
        normalized[i] = std::clamp(normalized_val, -1.0f, 1.0f);
    }
}

}  // namespace ml
}  // namespace edgeclear
