# EdgeClear Denoiser Model Assets

## Overview

This directory contains the TensorFlow Lite INT8 quantized denoiser model for EdgeClear's real-time audio enhancement pipeline.

## Model Requirements

### Architecture
- **Type**: TCN (Temporal Convolutional Network)
- **Input**: `[1, 161]` float32 (log-magnitude spectrum, normalized to [-1, 1])
- **Output**: `[1, 161]` float32 (gain mask, range [0, 1])
- **Quantization**: INT8 via QAT (Quantization-Aware Training)

### Performance
- **Target inference time**: <2 ms per 10 ms hop (on mid-range Android devices)
- **Delegates**: NNAPI (primary), XNNPACK (fallback), CPU (last resort)
- **CPU budget**: Must fit within 6 ms total DSP budget (AEC + RES + Denoiser + STFT/ISTFT)

### Training
- **Dataset**: DNS Challenge 2020 (or equivalent clean/noisy speech corpus)
- **Framework**: TensorFlow 2.14
- **Quantization**: QAT (not post-training quantization)
- **Target SI-SDR improvement**: >10 dB on validation set

## File Structure

```
models/
├── model_manifest.json          # Model metadata and SHA-256 hash
├── denoiser_tcn_int8.tflite    # INT8 quantized model (to be trained)
└── README.md                    # This file
```

## Creating the Model

### Step 1: Train the model

```python
# Example training script (pseudo-code)
import tensorflow as tf

# 1. Load DNS Challenge 2020 dataset
# 2. Extract STFT features (320-pt FFT, 160-sample hop)
# 3. Compute log-magnitude and normalize to [-1, 1]
# 4. Train TCN to predict ideal gain mask
# 5. Apply QAT during training
# 6. Export as .tflite with INT8 quantization
```

### Step 2: Convert to TFLite INT8

```python
converter = tf.lite.TFLiteConverter.from_keras_model(model)
converter.optimizations = [tf.lite.Optimize.DEFAULT]
converter.target_spec.supported_types = [tf.int8]
converter.inference_input_type = tf.int8
converter.inference_output_type = tf.int8

# Provide representative dataset for quantization
def representative_dataset():
    for sample in validation_dataset.take(100):
        yield [sample]

converter.representative_dataset = representative_dataset
tflite_model = converter.convert()

with open('denoiser_tcn_int8.tflite', 'wb') as f:
    f.write(tflite_model)
```

### Step 3: Compute SHA-256 hash

```bash
shasum -a 256 denoiser_tcn_int8.tflite
```

### Step 4: Update model_manifest.json

Replace the `placeholder_sha256_hash_will_be_computed_from_actual_model_file` with the actual hash.

## Model Validation

Before deploying, validate:

1. **Inference time**: Run on target devices and ensure <2 ms inference
2. **SI-SDR improvement**: Test on validation set, expect >10 dB improvement
3. **Delegate support**: Verify NNAPI/XNNPACK work correctly
4. **Robustness**: Test with various noise types (white, babble, traffic, etc.)

## Placeholder Status

⚠️ **Current Status: PLACEHOLDER**

The model file (`denoiser_tcn_int8.tflite`) does not exist yet. The denoiser will be disabled at runtime until a trained model is provided.

To enable the denoiser:
1. Train the model following the guidelines above
2. Place the `.tflite` file in this directory
3. Compute and update the SHA-256 hash in `model_manifest.json`
4. Rebuild the app

## Integration

The model is loaded in `SessionManager::InitializeDenoiser()` via:
- `DenoiserModelState::LoadModel()` - Loads .tflite and verifies SHA-256
- `DenoiserModelState::InitializeInterpreter()` - Sets up TFLite interpreter with delegate
- `DenoiserInference::ProcessSpectrum()` - Runs inference on 161-bin magnitude spectrum

See `ml/denoiser_model.h` for full API documentation.
