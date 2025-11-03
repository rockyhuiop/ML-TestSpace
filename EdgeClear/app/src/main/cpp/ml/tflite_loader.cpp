#include "denoiser_model.h"
#include <android/log.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>

#define LOG_TAG "EdgeClear-TFLite"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace edgeclear {
namespace ml {

DenoiserModelState::DenoiserModelState()
    : nnapi_delegate_(nullptr),
      xnnpack_delegate_(nullptr),
      delegate_type_(DelegateType::CPU_ONLY),
      is_initialized_(false),
      input_tensor_idx_(-1),
      output_tensor_idx_(-1),
      input_scale_(1.0f),
      input_zero_point_(0),
      output_scale_(1.0f),
      output_zero_point_(0) {
}

DenoiserModelState::~DenoiserModelState() {
    if (nnapi_delegate_) {
        TfLiteNnapiDelegateDelete(nnapi_delegate_);
    }
    if (xnnpack_delegate_) {
        TfLiteXNNPackDelegateDelete(xnnpack_delegate_);
    }
}

bool DenoiserModelState::LoadModel(const std::string& model_path,
                                    const std::string& expected_sha256) {
    LOGI("Loading denoiser model from: %s", model_path.c_str());

    // Verify SHA-256 integrity
    if (!VerifyModelHash(model_path, expected_sha256)) {
        LOGE("Model integrity check failed - SHA-256 mismatch");
        return false;
    }

    LOGI("Model integrity verified (SHA-256 match)");

    // Load FlatBuffer model
    model_ = tflite::FlatBufferModel::BuildFromFile(model_path.c_str());
    if (!model_) {
        LOGE("Failed to load model from file");
        return false;
    }

    LOGI("Model loaded successfully");
    return true;
}

bool DenoiserModelState::InitializeInterpreter() {
    if (!model_) {
        LOGE("Cannot initialize interpreter - model not loaded");
        return false;
    }

    // Build interpreter
    tflite::InterpreterBuilder builder(*model_, resolver_);
    builder(&interpreter_);

    if (!interpreter_) {
        LOGE("Failed to build interpreter");
        return false;
    }

    // Get tensor indices
    input_tensor_idx_ = interpreter_->inputs()[0];
    output_tensor_idx_ = interpreter_->outputs()[0];

    // Try delegate selection
    // Priority: NNAPI > XNNPACK > CPU-only
    if (TryNNAPIDelegate()) {
        delegate_type_ = DelegateType::NNAPI;
        LOGI("Using NNAPI delegate");
    } else if (TryXNNPACKDelegate()) {
        delegate_type_ = DelegateType::XNNPACK;
        LOGI("Using XNNPACK delegate (NNAPI unavailable)");
    } else {
        delegate_type_ = DelegateType::CPU_ONLY;
        LOGI("Using CPU-only (no delegates available)");
    }

    // Allocate tensors
    if (interpreter_->AllocateTensors() != kTfLiteOk) {
        LOGE("Failed to allocate tensors");
        return false;
    }

    // Extract quantization parameters
    ExtractQuantizationParams();

    is_initialized_ = true;
    LOGI("Interpreter initialized successfully");
    return true;
}

const char* DenoiserModelState::GetDelegateNameString() const {
    switch (delegate_type_) {
        case DelegateType::NNAPI:
            return "NNAPI";
        case DelegateType::XNNPACK:
            return "XNNPACK";
        case DelegateType::CPU_ONLY:
            return "CPU";
        default:
            return "Unknown";
    }
}

bool DenoiserModelState::VerifyModelHash(const std::string& model_path,
                                          const std::string& expected_sha256) {
    // Read file
    std::ifstream file(model_path, std::ios::binary);
    if (!file) {
        LOGE("Cannot open model file for hashing: %s", model_path.c_str());
        return false;
    }

    // Compute SHA-256
    SHA256_CTX sha256_ctx;
    SHA256_Init(&sha256_ctx);

    constexpr size_t kBufferSize = 4096;
    char buffer[kBufferSize];
    while (file.read(buffer, kBufferSize) || file.gcount() > 0) {
        SHA256_Update(&sha256_ctx, buffer, file.gcount());
    }

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &sha256_ctx);

    // Convert to hex string
    std::ostringstream oss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(hash[i]);
    }
    std::string computed_hash = oss.str();

    LOGI("Computed SHA-256: %s", computed_hash.c_str());
    LOGI("Expected SHA-256: %s", expected_sha256.c_str());

    return (computed_hash == expected_sha256);
}

bool DenoiserModelState::TryNNAPIDelegate() {
    // Try to create NNAPI delegate
    TfLiteNnapiDelegateOptions options = TfLiteNnapiDelegateOptionsDefault();
    options.allow_fp16 = true;  // Enable FP16 for better performance
    options.execution_preference = kTfLiteNnapiExecutionPreferenceFastSingleAnswer;

    nnapi_delegate_ = TfLiteNnapiDelegateCreate(&options);
    if (!nnapi_delegate_) {
        return false;
    }

    // Try to modify graph with delegate
    if (interpreter_->ModifyGraphWithDelegate(nnapi_delegate_) != kTfLiteOk) {
        TfLiteNnapiDelegateDelete(nnapi_delegate_);
        nnapi_delegate_ = nullptr;
        return false;
    }

    return true;
}

bool DenoiserModelState::TryXNNPACKDelegate() {
    // Try to create XNNPACK delegate
    TfLiteXNNPackDelegateOptions options = TfLiteXNNPackDelegateOptionsDefault();
    options.num_threads = 2;  // Use 2 threads for CPU inference

    xnnpack_delegate_ = TfLiteXNNPackDelegateCreate(&options);
    if (!xnnpack_delegate_) {
        return false;
    }

    // Try to modify graph with delegate
    if (interpreter_->ModifyGraphWithDelegate(xnnpack_delegate_) != kTfLiteOk) {
        TfLiteXNNPackDelegateDelete(xnnpack_delegate_);
        xnnpack_delegate_ = nullptr;
        return false;
    }

    return true;
}

void DenoiserModelState::ExtractQuantizationParams() {
    // Get input tensor quantization
    TfLiteTensor* input_tensor = interpreter_->tensor(input_tensor_idx_);
    if (input_tensor->quantization.type == kTfLiteAffineQuantization) {
        auto* quant_params = static_cast<TfLiteAffineQuantization*>(
            input_tensor->quantization.params);
        if (quant_params->scale && quant_params->zero_point) {
            input_scale_ = quant_params->scale->data[0];
            input_zero_point_ = quant_params->zero_point->data[0];
        }
    }

    // Get output tensor quantization
    TfLiteTensor* output_tensor = interpreter_->tensor(output_tensor_idx_);
    if (output_tensor->quantization.type == kTfLiteAffineQuantization) {
        auto* quant_params = static_cast<TfLiteAffineQuantization*>(
            output_tensor->quantization.params);
        if (quant_params->scale && quant_params->zero_point) {
            output_scale_ = quant_params->scale->data[0];
            output_zero_point_ = quant_params->zero_point->data[0];
        }
    }

    LOGI("Input quantization: scale=%.6f, zero_point=%d", input_scale_, input_zero_point_);
    LOGI("Output quantization: scale=%.6f, zero_point=%d", output_scale_, output_zero_point_);
}

}  // namespace ml
}  // namespace edgeclear
