#pragma once

#include <tensorflow/lite/interpreter.h>
#include <tensorflow/lite/kernels/register.h>
#include <tensorflow/lite/model.h>
#include <tensorflow/lite/delegates/nnapi/nnapi_delegate.h>
#include <tensorflow/lite/delegates/xnnpack/xnnpack_delegate.h>
#include <memory>
#include <string>

namespace edgeclear {
namespace ml {

/**
 * DenoiserModelState: TFLite INT8 denoiser model state
 *
 * Architecture:
 * - TCN (Temporal Convolutional Network) denoiser
 * - INT8 quantized (QAT trained)
 * - Input: 161 frequency bins (complex magnitude)
 * - Output: 161 gain values (0-1)
 *
 * Delegate selection:
 * - Primary: NNAPI (hardware accelerated)
 * - Fallback: XNNPACK (CPU optimized)
 *
 * Constitution compliance:
 * - Model integrity: SHA-256 verification on load
 * - CPU budget: <2 ms inference time (within 6 ms total budget)
 * - RT-safe: No allocations during inference
 *
 * References:
 * - TFLite API: https://www.tensorflow.org/lite/api_docs/cc
 * - NNAPI delegate: https://www.tensorflow.org/lite/performance/nnapi
 */
class DenoiserModelState {
public:
    static constexpr int kNumBins = 161;  // 320-point FFT / 2 + 1
    static constexpr int kInputSize = kNumBins;
    static constexpr int kOutputSize = kNumBins;

    enum class DelegateType {
        NNAPI,
        XNNPACK,
        CPU_ONLY
    };

    DenoiserModelState();
    ~DenoiserModelState();

    // Non-copyable
    DenoiserModelState(const DenoiserModelState&) = delete;
    DenoiserModelState& operator=(const DenoiserModelState&) = delete;

    /**
     * Load model from file with SHA-256 verification.
     *
     * @param model_path Path to .tflite file
     * @param expected_sha256 Expected SHA-256 hash (64 hex chars)
     * @return true on success
     */
    bool LoadModel(const std::string& model_path, const std::string& expected_sha256);

    /**
     * Initialize interpreter with delegate selection.
     * Tries NNAPI first, falls back to XNNPACK if unavailable.
     *
     * @return true on success
     */
    bool InitializeInterpreter();

    /**
     * Get active delegate type.
     */
    DelegateType GetDelegateType() const { return delegate_type_; }

    /**
     * Get delegate name string for logging.
     */
    const char* GetDelegateNameString() const;

    /**
     * Check if model is loaded and ready.
     */
    bool IsReady() const { return model_ && interpreter_ && is_initialized_; }

    // TFLite objects (exposed for inference wrapper)
    tflite::Interpreter* GetInterpreter() { return interpreter_.get(); }

    /**
     * Get input tensor index.
     */
    int GetInputTensorIndex() const { return input_tensor_idx_; }

    /**
     * Get output tensor index.
     */
    int GetOutputTensorIndex() const { return output_tensor_idx_; }

    /**
     * Get input quantization parameters.
     */
    float GetInputScale() const { return input_scale_; }
    int32_t GetInputZeroPoint() const { return input_zero_point_; }

    /**
     * Get output quantization parameters.
     */
    float GetOutputScale() const { return output_scale_; }
    int32_t GetOutputZeroPoint() const { return output_zero_point_; }

private:
    // TFLite components
    std::unique_ptr<tflite::FlatBufferModel> model_;
    std::unique_ptr<tflite::Interpreter> interpreter_;
    tflite::ops::builtin::BuiltinOpResolver resolver_;

    // Delegate
    TfLiteDelegate* nnapi_delegate_;
    TfLiteDelegate* xnnpack_delegate_;
    DelegateType delegate_type_;

    // Model state
    bool is_initialized_;
    int input_tensor_idx_;
    int output_tensor_idx_;

    // Quantization parameters (INT8)
    float input_scale_;
    int32_t input_zero_point_;
    float output_scale_;
    int32_t output_zero_point_;

    // Helper methods
    bool VerifyModelHash(const std::string& model_path, const std::string& expected_sha256);
    bool TryNNAPIDelegate();
    bool TryXNNPACKDelegate();
    void ExtractQuantizationParams();
};

}  // namespace ml
}  // namespace edgeclear
