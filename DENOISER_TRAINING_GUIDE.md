# EdgeClear M3 Denoiser Training Guide

**Status**: M3 Infrastructure Complete - Ready for Model Training
**Date**: November 3, 2025
**Version**: 1.0

---

## Executive Summary

The EdgeClear Android audio enhancement pipeline is **fully operational** and ready for denoiser model training. All blocking issues have been resolved, and the TFLite INT8 inference pipeline is integrated and tested.

**What's Complete:**
- ✅ TFLite model loading with SHA-256 verification
- ✅ NNAPI/XNNPACK/CPU delegate selection
- ✅ INT8 quantization/dequantization pipeline
- ✅ Full DSP pipeline integration (AEC → RES → Denoiser → Output)
- ✅ Android AssetManager integration
- ✅ SI-SDR metrics computation and UI display
- ✅ Graceful degradation (runs without model)

**What's Needed:**
- 🔴 Trained INT8 TFLite denoiser model
- 🔴 Model validation on target hardware
- 🔴 SHA-256 hash update in manifest

---

## Architecture Overview

### Audio Pipeline (48 kHz I/O, 16 kHz Processing)

```
Microphone (48 kHz)
    ↓
AAudio Capture (RT thread)
    ↓
Decimate 48→16 kHz (SimpleResampler)
    ↓
input_queue (SPSC lock-free)
    ↓
═══════════════════════════════════════
DSP Worker Thread (Non-RT):
═══════════════════════════════════════
    1. AEC (Acoustic Echo Cancellation)
       - Time domain, 160 samples
       - FD-NLMS, 8 partitions, 160 ms tail
       ↓
    2. STFT (320-point FFT)
       - 50% overlap, Hann window
       - Output: 161 complex bins
       ↓
    3. DTD + RES (Double-Talk + Residual Echo)
       - Coherence-based detection
       - Wiener-like suppression
       ↓
    4. ⭐ DENOISER (THIS IS WHERE YOUR MODEL RUNS) ⭐
       - Input: 161 float32 magnitudes (normalized to [-1, 1])
       - Inference: INT8 TFLite model
       - Output: 161 float32 gains (range [0, 1])
       - Budget: <2 ms per 10 ms hop
       ↓
    5. ISTFT (320-point inverse FFT)
       - Overlap-add reconstruction
       - Output: 160 time-domain samples
       ↓
═══════════════════════════════════════
output_queue (SPSC lock-free)
    ↓
Interpolate 16→48 kHz (SimpleResampler)
    ↓
AAudio Render (RT thread)
    ↓
Speaker (48 kHz)
```

### Denoiser Specifications

**Input Format:**
- **Shape**: `[1, 161]` (batch size 1, 161 frequency bins)
- **Type**: INT8 (quantized from float32)
- **Range**: Normalized log-magnitude spectrum in [-1, 1]
- **Normalization Formula**:
  ```python
  log_mag = log10(max(magnitude, 1e-6))
  normalized = (log_mag - (-6.0)) / (4.5 - (-6.0)) * 2.0 - 1.0
  clipped = clip(normalized, -1.0, 1.0)
  ```

**Output Format:**
- **Shape**: `[1, 161]` (batch size 1, 161 frequency bins)
- **Type**: INT8 (dequantized to float32)
- **Range**: Gain mask in [0, 1]
- **Application**: `output_spectrum[i] = input_spectrum[i] * gain[i]`

**Performance Requirements:**
- **Target inference time**: <2 ms per hop
- **Total DSP budget**: <6 ms per 10 ms hop
- **Tested devices**: Snapdragon 778G, Pixel 6/7
- **Delegates**: NNAPI (primary), XNNPACK (fallback), CPU (last resort)

---

## Model Training Pipeline

### 1. Environment Setup

```bash
# Create virtual environment
conda create -n edgeclear-train python=3.9
conda activate edgeclear-train

# Install TensorFlow and dependencies
pip install tensorflow==2.14.0
pip install tensorflow-model-optimization
pip install soundfile
pip install scipy
pip install tqdm
pip install matplotlib
pip install librosa
```

### 2. Dataset Preparation

**Recommended Dataset**: DNS Challenge 2020

```bash
# Download DNS Challenge dataset
wget https://dns-challenge.s3.us-east-2.amazonaws.com/datasets/dataset_4.tar.gz
tar -xzf dataset_4.tar.gz

# Directory structure:
# dns_challenge_data/
#   clean_train/  (clean speech)
#   noise_train/  (various noise types)
#   clean_val/
#   noise_val/
```

**Alternative Datasets**:
- Librispeech (clean speech)
- UrbanSound8K (noise)
- ESC-50 (environmental sounds)

### 3. Model Architecture

**TCN (Temporal Convolutional Network)** - Recommended

```python
# Pseudocode architecture
Input: [batch, sequence, 161 bins]
    ↓
Conv1D(128 filters, kernel=3, dilation=1) → LayerNorm → ReLU
    ↓
Conv1D(128 filters, kernel=3, dilation=2) → LayerNorm → ReLU
    ↓
Conv1D(64 filters, kernel=3, dilation=4) → LayerNorm → ReLU
    ↓
Conv1D(64 filters, kernel=3, dilation=8) → LayerNorm → ReLU
    ↓
Conv1D(161 filters, kernel=1, activation=sigmoid)
    ↓
Output: [batch, sequence, 161 bins] (gain mask)
```

**Key Points:**
- Use **Quantization-Aware Training (QAT)** - not post-training quantization
- Sequence length: 50-100 hops (~0.5-1 second)
- Causal padding for real-time processing
- Dropout (0.1-0.2) to prevent overfitting

### 4. Training Configuration

```python
# Hyperparameters
SAMPLE_RATE = 16000
FFT_SIZE = 320
HOP_SIZE = 160
NUM_BINS = 161

BATCH_SIZE = 32
EPOCHS = 100
LEARNING_RATE = 1e-3
SEQUENCE_LENGTH = 100  # 1 second

SNR_RANGE = (-5, 20)  # dB
```

**Loss Function**: Mean Squared Error (MSE) on gain masks

**Data Augmentation**:
- Random SNR mixing
- Random noise types
- Random speech segments
- On-the-fly STFT computation

### 5. Quantization-Aware Training (QAT)

```python
import tensorflow_model_optimization as tfmot

# Apply QAT to model
quantize_model = tfmot.quantization.keras.quantize_model
q_aware_model = quantize_model(base_model)

# Train with QAT
q_aware_model.compile(
    optimizer=tf.keras.optimizers.Adam(1e-3),
    loss='mse',
    metrics=['mae']
)

q_aware_model.fit(train_dataset, validation_data=val_dataset, epochs=100)
```

### 6. TFLite INT8 Conversion

```python
def convert_to_int8(model):
    # Representative dataset for quantization calibration
    def representative_dataset():
        for _ in range(100):
            X = generate_random_batch()  # [1, 100, 161]
            yield [X.astype(np.float32)]

    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.target_spec.supported_types = [tf.int8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8
    converter.representative_dataset = representative_dataset

    tflite_model = converter.convert()

    with open('denoiser_tcn_int8.tflite', 'wb') as f:
        f.write(tflite_model)

    return tflite_model
```

### 7. Model Validation

```python
# Test inference time
import time

interpreter = tf.lite.Interpreter(model_path='denoiser_tcn_int8.tflite')
interpreter.allocate_tensors()

input_details = interpreter.get_input_details()
output_details = interpreter.get_output_details()

# Warm-up
for _ in range(10):
    test_input = np.random.randint(-128, 127, (1, 100, 161), dtype=np.int8)
    interpreter.set_tensor(input_details[0]['index'], test_input)
    interpreter.invoke()

# Measure inference time
times = []
for _ in range(100):
    test_input = np.random.randint(-128, 127, (1, 100, 161), dtype=np.int8)

    start = time.perf_counter()
    interpreter.set_tensor(input_details[0]['index'], test_input)
    interpreter.invoke()
    end = time.perf_counter()

    times.append((end - start) * 1000)  # Convert to ms

print(f"Mean inference time: {np.mean(times):.2f} ms")
print(f"95th percentile: {np.percentile(times, 95):.2f} ms")

# MUST BE <2 ms for real-time processing
assert np.mean(times) < 2.0, "Model too slow!"
```

---

## Deployment Workflow

### Step 1: Compute SHA-256 Hash

```bash
cd /path/to/trained/model
shasum -a 256 denoiser_tcn_int8.tflite

# Output example:
# 3f8a2b9c4d5e6f7a8b9c0d1e2f3a4b5c6d7e8f9a0b1c2d3e4f5a6b7c8d9e0f1a2  denoiser_tcn_int8.tflite
```

### Step 2: Copy Model to Assets

```bash
cp denoiser_tcn_int8.tflite \
   /Users/roc_op/testspace/ML-TestSpace/EdgeClear/app/src/main/assets/models/
```

### Step 3: Update Manifest

Edit: `EdgeClear/app/src/main/assets/models/model_manifest.json`

```json
{
  "version": "1.0.0",
  "models": {
    "denoiser_int8_v1": {
      "filename": "denoiser_tcn_int8.tflite",
      "sha256": "3f8a2b9c4d5e6f7a8b9c0d1e2f3a4b5c6d7e8f9a0b1c2d3e4f5a6b7c8d9e0f1a2",
      "description": "TCN-based denoiser, INT8 quantized, 161 frequency bins",
      "architecture": {
        "type": "TCN",
        "input_shape": [1, 161],
        "output_shape": [1, 161],
        "quantization": "INT8"
      },
      "performance": {
        "target_inference_time_ms": 2.0,
        "delegates": ["NNAPI", "XNNPACK", "CPU"]
      },
      "status": "trained"
    }
  }
}
```

### Step 4: Build and Deploy

```bash
cd /Users/roc_op/testspace/ML-TestSpace/EdgeClear
./gradlew clean
./gradlew assembleDebug
./gradlew installDebug

# Monitor logs
adb logcat -s EdgeClear-Session EdgeClear-TFLite EdgeClear-Denoiser
```

**Expected Log Output:**

```
EdgeClear-Session: Initializing denoiser model from assets
EdgeClear-TFLite: Loading denoiser model from: /data/local/tmp/denoiser_tcn_int8.tflite
EdgeClear-TFLite: Computed SHA-256: 3f8a2b9c4d5e6f7a8b9c0d1e2f3a4b5c...
EdgeClear-TFLite: Expected SHA-256: 3f8a2b9c4d5e6f7a8b9c0d1e2f3a4b5c...
EdgeClear-TFLite: Model integrity verified (SHA-256 match)
EdgeClear-TFLite: Model loaded successfully
EdgeClear-TFLite: Using NNAPI delegate
EdgeClear-Session: Denoiser model loaded successfully
EdgeClear-Session:   Delegate: NNAPI
EdgeClear-Session:   Input quantization: scale=0.007843, zero_point=-128
EdgeClear-Session:   Output quantization: scale=0.003922, zero_point=0
EdgeClear-DSPWorker: DSPWorker initialized (AEC enabled, Denoiser enabled)
EdgeClear-DSPWorker: Denoiser initialized and enabled
```

### Step 5: Validate Performance

```bash
# Check CPU time (should be <6 ms per hop)
adb logcat -s EdgeClear-DSPWorker | grep "CPU"

# Check SI-SDR improvement (should be positive)
# Look at MetricsOverlay UI in app

# Profile on device
adb shell simpleperf record -p $(adb shell pidof com.edgeclear) -o /data/local/tmp/perf.data
adb pull /data/local/tmp/perf.data
simpleperf report -i perf.data
```

---

## Complete Training Script

Save as `train_edgeclear_denoiser.py`:

```python
#!/usr/bin/env python3
"""
EdgeClear INT8 Denoiser Training Script

Trains a TCN-based denoiser model with Quantization-Aware Training (QAT)
for deployment on Android devices with EdgeClear audio pipeline.

Requirements:
  - TensorFlow 2.14.0
  - DNS Challenge 2020 dataset
  - 16GB+ RAM
  - GPU recommended (training takes ~4-6 hours)

Usage:
  python train_edgeclear_denoiser.py \\
      --clean_dir ./dns_data/clean_train \\
      --noise_dir ./dns_data/noise_train \\
      --output_path ./denoiser_tcn_int8.tflite
"""

import argparse
import numpy as np
import tensorflow as tf
import soundfile as sf
from pathlib import Path
from tqdm import tqdm
import time

# EdgeClear audio pipeline configuration
SAMPLE_RATE = 16000
FFT_SIZE = 320
HOP_SIZE = 160
NUM_BINS = 161  # FFT_SIZE // 2 + 1

# Training configuration
BATCH_SIZE = 32
EPOCHS = 100
LEARNING_RATE = 1e-3
SEQUENCE_LENGTH = 100  # 100 hops = 1 second

# Magnitude normalization (matches C++ implementation)
LOG_MIN = -6.0  # log10(1e-6) - silence floor
LOG_MAX = 4.5   # log10(32767) - full scale


class DNSDataGenerator:
    """
    Data generator for DNS Challenge dataset.
    Generates training pairs: (noisy magnitude spectrum → ideal gain mask)
    """

    def __init__(self, clean_dir, noise_dir, snr_range=(-5, 20)):
        self.clean_files = list(Path(clean_dir).glob("*.wav"))
        self.noise_files = list(Path(noise_dir).glob("*.wav"))
        self.snr_range = snr_range

        print(f"Found {len(self.clean_files)} clean files")
        print(f"Found {len(self.noise_files)} noise files")

        if len(self.clean_files) == 0 or len(self.noise_files) == 0:
            raise ValueError("No audio files found in dataset directories")

    def load_audio(self, path):
        """Load audio file and resample to 16 kHz if needed"""
        audio, sr = sf.read(path)

        # Convert stereo to mono
        if len(audio.shape) > 1:
            audio = audio.mean(axis=1)

        # Resample if needed
        if sr != SAMPLE_RATE:
            from scipy import signal
            audio = signal.resample_poly(audio, SAMPLE_RATE, sr)

        return audio.astype(np.float32)

    def mix_audio(self, clean, noise, snr_db):
        """Mix clean speech and noise at specified SNR"""
        # Calculate power
        clean_power = np.mean(clean ** 2)
        noise_power = np.mean(noise ** 2)

        # Calculate noise scaling factor
        snr_linear = 10 ** (snr_db / 10)
        noise_scale = np.sqrt(clean_power / (snr_linear * noise_power + 1e-10))

        # Match lengths
        if len(noise) < len(clean):
            # Tile noise to match clean length
            noise = np.tile(noise, len(clean) // len(noise) + 1)[:len(clean)]
        else:
            # Random crop noise
            start = np.random.randint(0, len(noise) - len(clean))
            noise = noise[start:start + len(clean)]

        # Mix
        noisy = clean + noise_scale * noise

        return noisy, clean

    def compute_stft(self, audio):
        """Compute STFT with Hann window (matches C++ pipeline)"""
        window = np.hanning(FFT_SIZE)

        num_frames = (len(audio) - FFT_SIZE) // HOP_SIZE + 1
        stft = np.zeros((num_frames, NUM_BINS), dtype=np.complex64)

        for i in range(num_frames):
            start = i * HOP_SIZE
            frame = audio[start:start + FFT_SIZE] * window
            fft = np.fft.rfft(frame, n=FFT_SIZE)
            stft[i] = fft

        return stft

    def normalize_magnitude(self, magnitude):
        """
        Normalize magnitude to [-1, 1] range.
        Matches C++ implementation in denoiser_inference.cpp
        """
        log_mag = np.log10(np.maximum(magnitude, 1e-6))
        normalized = (log_mag - LOG_MIN) / (LOG_MAX - LOG_MIN) * 2.0 - 1.0
        normalized = np.clip(normalized, -1.0, 1.0)
        return normalized

    def compute_ideal_gain(self, clean_stft, noisy_stft):
        """Compute ideal Wiener-like gain mask"""
        clean_mag = np.abs(clean_stft)
        noisy_mag = np.abs(noisy_stft)

        # Wiener gain
        gain = clean_mag / (noisy_mag + 1e-10)
        gain = np.clip(gain, 0.0, 1.0)

        return gain.astype(np.float32)

    def generate_batch(self, batch_size):
        """Generate a batch of training examples"""
        inputs = []
        targets = []

        for _ in range(batch_size):
            # Random files
            clean_path = np.random.choice(self.clean_files)
            noise_path = np.random.choice(self.noise_files)

            # Load audio
            clean = self.load_audio(clean_path)
            noise = self.load_audio(noise_path)

            # Skip if too short
            if len(clean) < FFT_SIZE * 2 or len(noise) < FFT_SIZE * 2:
                continue

            # Random SNR
            snr = np.random.uniform(*self.snr_range)

            # Mix
            noisy, clean = self.mix_audio(clean, noise, snr)

            # STFT
            clean_stft = self.compute_stft(clean)
            noisy_stft = self.compute_stft(noisy)

            # Features and targets
            noisy_mag = np.abs(noisy_stft)
            noisy_mag_norm = self.normalize_magnitude(noisy_mag)
            ideal_gain = self.compute_ideal_gain(clean_stft, noisy_stft)

            # Random segment
            if len(noisy_mag_norm) > SEQUENCE_LENGTH:
                start = np.random.randint(0, len(noisy_mag_norm) - SEQUENCE_LENGTH)
                noisy_mag_norm = noisy_mag_norm[start:start + SEQUENCE_LENGTH]
                ideal_gain = ideal_gain[start:start + SEQUENCE_LENGTH]
            else:
                # Pad if too short
                pad_len = SEQUENCE_LENGTH - len(noisy_mag_norm)
                noisy_mag_norm = np.pad(noisy_mag_norm, ((0, pad_len), (0, 0)))
                ideal_gain = np.pad(ideal_gain, ((0, pad_len), (0, 0)))

            inputs.append(noisy_mag_norm)
            targets.append(ideal_gain)

        return np.array(inputs, dtype=np.float32), np.array(targets, dtype=np.float32)


def build_tcn_denoiser():
    """Build TCN (Temporal Convolutional Network) denoiser model"""
    from tensorflow.keras import layers, Model

    inputs = layers.Input(shape=(SEQUENCE_LENGTH, NUM_BINS), name='magnitude_input')

    x = inputs

    # TCN blocks with increasing dilation
    filters = [128, 128, 64, 64]
    for i, num_filters in enumerate(filters):
        dilation_rate = 2 ** i

        # Dilated causal convolution
        conv = layers.Conv1D(
            filters=num_filters,
            kernel_size=3,
            dilation_rate=dilation_rate,
            padding='causal',
            activation=None,
            name=f'conv_{i}'
        )(x)

        conv = layers.LayerNormalization(name=f'norm_{i}')(conv)
        conv = layers.Activation('relu', name=f'relu_{i}')(conv)
        conv = layers.Dropout(0.1, name=f'dropout_{i}')(conv)

        # Residual connection
        if x.shape[-1] == num_filters:
            x = layers.Add(name=f'residual_{i}')([x, conv])
        else:
            # Project to match dimensions
            residual = layers.Conv1D(num_filters, 1, name=f'project_{i}')(x)
            x = layers.Add(name=f'residual_{i}')([residual, conv])

    # Output layer - gain mask per frequency bin
    outputs = layers.Conv1D(
        filters=NUM_BINS,
        kernel_size=1,
        activation='sigmoid',
        name='gain_output'
    )(x)

    model = Model(inputs=inputs, outputs=outputs, name='tcn_denoiser')
    return model


def train_with_qat(train_gen, val_gen, output_path):
    """Train model with Quantization-Aware Training"""
    import tensorflow_model_optimization as tfmot

    print("\n=== Building Model ===")
    base_model = build_tcn_denoiser()
    base_model.summary()

    print("\n=== Applying Quantization-Aware Training ===")
    quantize_model = tfmot.quantization.keras.quantize_model
    q_aware_model = quantize_model(base_model)

    q_aware_model.compile(
        optimizer=tf.keras.optimizers.Adam(LEARNING_RATE),
        loss='mse',
        metrics=['mae']
    )

    print("\n=== Training with QAT ===")
    best_val_loss = float('inf')

    for epoch in range(EPOCHS):
        print(f"\nEpoch {epoch + 1}/{EPOCHS}")

        # Training
        train_losses = []
        for batch_idx in tqdm(range(100), desc="Training"):
            X, y = train_gen.generate_batch(BATCH_SIZE)
            loss, mae = q_aware_model.train_on_batch(X, y)
            train_losses.append(loss)

        # Validation
        val_losses = []
        for batch_idx in range(20):
            X, y = val_gen.generate_batch(BATCH_SIZE)
            loss, mae = q_aware_model.test_on_batch(X, y)
            val_losses.append(loss)

        train_loss = np.mean(train_losses)
        val_loss = np.mean(val_losses)

        print(f"Train Loss: {train_loss:.4f}, Val Loss: {val_loss:.4f}")

        # Save best model
        if val_loss < best_val_loss:
            best_val_loss = val_loss
            print(f"New best model! Saving checkpoint...")
            q_aware_model.save('best_model.h5')

        # Checkpoint every 10 epochs
        if (epoch + 1) % 10 == 0:
            q_aware_model.save(f'checkpoint_epoch_{epoch + 1}.h5')

    return q_aware_model


def convert_to_tflite_int8(model, val_gen, output_path):
    """Convert trained model to TFLite INT8 format"""

    print("\n=== Converting to TFLite INT8 ===")

    # Representative dataset for quantization calibration
    def representative_dataset():
        print("Generating representative dataset...")
        for _ in tqdm(range(100)):
            X, _ = val_gen.generate_batch(1)
            yield [X.astype(np.float32)]

    # Convert
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.target_spec.supported_types = [tf.int8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8
    converter.representative_dataset = representative_dataset

    tflite_model = converter.convert()

    # Save
    with open(output_path, 'wb') as f:
        f.write(tflite_model)

    print(f"\n✓ Model saved to: {output_path}")
    print(f"✓ Model size: {len(tflite_model) / 1024:.2f} KB")

    return tflite_model


def validate_inference_time(model_path):
    """Validate inference time on CPU"""
    print("\n=== Validating Inference Time ===")

    interpreter = tf.lite.Interpreter(model_path=model_path)
    interpreter.allocate_tensors()

    input_details = interpreter.get_input_details()
    output_details = interpreter.get_output_details()

    # Warm-up
    for _ in range(10):
        test_input = np.random.randint(-128, 127, (1, SEQUENCE_LENGTH, NUM_BINS), dtype=np.int8)
        interpreter.set_tensor(input_details[0]['index'], test_input)
        interpreter.invoke()

    # Measure
    times = []
    for _ in range(100):
        test_input = np.random.randint(-128, 127, (1, SEQUENCE_LENGTH, NUM_BINS), dtype=np.int8)

        start = time.perf_counter()
        interpreter.set_tensor(input_details[0]['index'], test_input)
        interpreter.invoke()
        end = time.perf_counter()

        times.append((end - start) * 1000)

    mean_time = np.mean(times)
    p95_time = np.percentile(times, 95)

    print(f"Mean inference time: {mean_time:.2f} ms")
    print(f"95th percentile: {p95_time:.2f} ms")

    if mean_time < 2.0:
        print("✓ Model meets <2 ms requirement")
    else:
        print("✗ Model too slow - needs optimization")

    return mean_time < 2.0


def main():
    parser = argparse.ArgumentParser(description='Train EdgeClear INT8 Denoiser')
    parser.add_argument('--clean_dir', required=True, help='Clean speech directory')
    parser.add_argument('--noise_dir', required=True, help='Noise directory')
    parser.add_argument('--clean_val_dir', help='Validation clean speech directory')
    parser.add_argument('--noise_val_dir', help='Validation noise directory')
    parser.add_argument('--output_path', default='denoiser_tcn_int8.tflite',
                        help='Output TFLite model path')

    args = parser.parse_args()

    # Use same dirs for validation if not specified
    clean_val_dir = args.clean_val_dir or args.clean_dir
    noise_val_dir = args.noise_val_dir or args.noise_dir

    # Create data generators
    print("\n=== Loading Dataset ===")
    train_gen = DNSDataGenerator(args.clean_dir, args.noise_dir)
    val_gen = DNSDataGenerator(clean_val_dir, noise_val_dir)

    # Train with QAT
    model = train_with_qat(train_gen, val_gen, args.output_path)

    # Convert to TFLite INT8
    convert_to_tflite_int8(model, val_gen, args.output_path)

    # Validate inference time
    is_fast_enough = validate_inference_time(args.output_path)

    if is_fast_enough:
        print("\n✓ Training complete! Model ready for deployment.")
        print(f"\nNext steps:")
        print(f"1. Compute SHA-256: shasum -a 256 {args.output_path}")
        print(f"2. Copy to assets: cp {args.output_path} EdgeClear/app/src/main/assets/models/")
        print(f"3. Update model_manifest.json with SHA-256 hash")
        print(f"4. Build and deploy: cd EdgeClear && ./gradlew installDebug")
    else:
        print("\n✗ Model too slow - consider:")
        print("  - Reducing number of filters")
        print("  - Reducing sequence length")
        print("  - Using fewer TCN blocks")


if __name__ == "__main__":
    main()
```

---

## Quick Start (Minimal Testing Model)

If you need a placeholder model immediately for testing:

```python
# generate_dummy_model.py
import tensorflow as tf
import numpy as np

# Simple pass-through model
model = tf.keras.Sequential([
    tf.keras.layers.Input(shape=(100, 161)),
    tf.keras.layers.Conv1D(64, 3, padding='same', activation='relu'),
    tf.keras.layers.Conv1D(161, 1, activation='sigmoid')
])

model.compile(optimizer='adam', loss='mse')

# Dummy training
X = np.random.randn(100, 100, 161).astype(np.float32)
y = np.random.rand(100, 100, 161).astype(np.float32)
model.fit(X, y, epochs=1, batch_size=32, verbose=0)

# Convert to INT8
def rep_dataset():
    for _ in range(100):
        yield [np.random.randn(1, 100, 161).astype(np.float32)]

converter = tf.lite.TFLiteConverter.from_keras_model(model)
converter.optimizations = [tf.lite.Optimize.DEFAULT]
converter.representative_dataset = rep_dataset
converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
converter.inference_input_type = tf.int8
converter.inference_output_type = tf.int8

tflite_model = converter.convert()

with open('denoiser_tcn_int8.tflite', 'wb') as f:
    f.write(tflite_model)

# Compute SHA-256
import hashlib
sha256 = hashlib.sha256(tflite_model).hexdigest()
print(f"SHA-256: {sha256}")
print(f"Size: {len(tflite_model) / 1024:.2f} KB")
print("\nDummy model created for pipeline testing!")
```

---

## Troubleshooting

### Issue: Model loads but denoiser not enabled

**Check logs:**
```bash
adb logcat -s EdgeClear-DSPWorker | grep -i denoiser
```

**Possible causes:**
- SHA-256 mismatch (check manifest)
- Model file not in assets
- AssetManager not passed to JNI

### Issue: Inference too slow

**Profile on device:**
```bash
adb shell simpleperf record -p $(adb shell pidof com.edgeclear)
adb pull /data/local/tmp/perf.data
```

**Solutions:**
- Reduce model complexity
- Enable NNAPI delegate
- Test on newer device (Snapdragon 888+)

### Issue: Poor audio quality

**Check SI-SDR metric:**
- Should be positive (indicates improvement)
- If negative, model making audio worse

**Solutions:**
- Train on more diverse dataset
- Increase training epochs
- Adjust gain computation (try spectral subtraction)

---

## References

- **TFLite Quantization Guide**: https://www.tensorflow.org/lite/performance/post_training_quantization
- **DNS Challenge Dataset**: https://github.com/microsoft/DNS-Challenge
- **TCN Paper**: "Temporal Convolutional Networks" (Bai et al., 2018)
- **EdgeClear Source**: `/Users/roc_op/testspace/ML-TestSpace/EdgeClear/`

---

## Contact & Support

For questions or issues:
1. Check `EdgeClear/app/src/main/assets/models/README.md`
2. Review logs: `adb logcat -s EdgeClear-*`
3. File issue with inference time profile

**Status**: Ready for production model training ✅
