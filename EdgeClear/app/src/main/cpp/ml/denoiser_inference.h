#pragma once

#include "denoiser_model.h"
#include <memory>

namespace edgeclear {
namespace ml {

/**
 * DenoiserInference: Real-time INT8 denoiser inference wrapper
 *
 * Wraps TFLite interpreter for RT-safe inference:
 * - Input: 161 float32 magnitude values (STFT bins)
 * - Process: Quantize → Inference → Dequantize
 * - Output: 161 float32 gain values (0-1 range)
 *
 * Constitution requirements:
 * - <2 ms inference time (within 6 ms total DSP budget)
 * - No allocations during inference (pre-allocated buffers)
 * - Quantization-aware: Uses model's scale/zero-point params
 *
 * Usage:
 *   DenoiserInference denoiser;
 *   denoiser.Initialize(model_state);
 *
 *   // In RT loop:
 *   denoiser.ProcessSpectrum(magnitude_bins, gain_output);
 */
class DenoiserInference {
public:
    DenoiserInference();
    ~DenoiserInference() = default;

    // Non-copyable
    DenoiserInference(const DenoiserInference&) = delete;
    DenoiserInference& operator=(const DenoiserInference&) = delete;

    /**
     * Initialize inference engine with loaded model.
     * Must be called before ProcessSpectrum().
     *
     * @param model_state Loaded and initialized model state
     * @return true on success
     */
    bool Initialize(DenoiserModelState* model_state);

    /**
     * Process spectrum through denoiser (RT-safe).
     *
     * Input format:
     *   - magnitude[i] = sqrt(real[i]^2 + imag[i]^2) for i in [0, 160]
     *   - Typically in range [0, ~32767] for int16 audio
     *
     * Output format:
     *   - gain[i] in range [0, 1]
     *   - Apply as: output_spectrum[i] = input_spectrum[i] * gain[i]
     *
     * @param magnitude Input spectrum magnitudes (161 bins)
     * @param gain Output gain values (161 bins)
     * @return true on success
     */
    bool ProcessSpectrum(const float* magnitude, float* gain);

    /**
     * Check if inference is ready.
     */
    bool IsReady() const { return is_initialized_; }

    /**
     * Get last inference time in microseconds.
     */
    int64_t GetLastInferenceTimeUs() const { return last_inference_time_us_; }

private:
    DenoiserModelState* model_state_;  // Not owned
    bool is_initialized_;
    int64_t last_inference_time_us_;

    // Pre-allocated inference buffers (avoid RT allocations)
    int8_t input_quantized_[DenoiserModelState::kInputSize];
    int8_t output_quantized_[DenoiserModelState::kOutputSize];

    /**
     * Quantize float32 input to INT8.
     * Formula: q = clip(round(f / scale) + zero_point, -128, 127)
     */
    void QuantizeInput(const float* input, int8_t* output,
                       float scale, int32_t zero_point, int size);

    /**
     * Dequantize INT8 output to float32.
     * Formula: f = (q - zero_point) * scale
     */
    void DequantizeOutput(const int8_t* input, float* output,
                          float scale, int32_t zero_point, int size);

    /**
     * Normalize input magnitude to model's expected range.
     * Denoiser was trained on normalized log-magnitude spectra.
     */
    void NormalizeMagnitude(const float* magnitude, float* normalized, int size);
};

}  // namespace ml
}  // namespace edgeclear
