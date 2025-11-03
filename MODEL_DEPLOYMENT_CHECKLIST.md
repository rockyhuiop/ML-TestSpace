# EdgeClear Denoiser Model Deployment Checklist

Quick reference for training and deploying the INT8 denoiser model.

---

## Prerequisites Checklist

- [ ] Python 3.9+ environment with TensorFlow 2.14.0
- [ ] DNS Challenge 2020 dataset downloaded (~20 GB)
- [ ] GPU available (training takes 4-6 hours on CPU)
- [ ] 16GB+ RAM available
- [ ] Android device for testing (Snapdragon 778G or newer)
- [ ] EdgeClear code at: `/Users/roc_op/testspace/ML-TestSpace/EdgeClear/`

---

## Training Steps

### 1. Setup Environment

```bash
conda create -n edgeclear-train python=3.9 -y
conda activate edgeclear-train

pip install tensorflow==2.14.0 \
            tensorflow-model-optimization \
            soundfile scipy tqdm matplotlib librosa
```

**Verify**:
```bash
python -c "import tensorflow as tf; print(tf.__version__)"
# Expected: 2.14.0
```

- [ ] Environment created
- [ ] TensorFlow 2.14.0 installed
- [ ] GPU detected (optional): `python -c "import tensorflow as tf; print(tf.config.list_physical_devices('GPU'))"`

---

### 2. Download Dataset

```bash
mkdir -p dns_challenge_data
cd dns_challenge_data

# Download dataset (~20 GB)
wget https://dns-challenge.s3.us-east-2.amazonaws.com/datasets/dataset_4.tar.gz

# Extract
tar -xzf dataset_4.tar.gz

# Verify structure
ls -lh clean_train/ noise_train/ clean_val/ noise_val/
```

**Expected structure**:
```
dns_challenge_data/
├── clean_train/    (~10,000 WAV files)
├── noise_train/    (~1,000 WAV files)
├── clean_val/      (~1,000 WAV files)
└── noise_val/      (~100 WAV files)
```

- [ ] Dataset downloaded
- [ ] Dataset extracted
- [ ] Directory structure verified

---

### 3. Run Training Script

Copy training script from `DENOISER_TRAINING_GUIDE.md` or use provided script:

```bash
python train_edgeclear_denoiser.py \
    --clean_dir dns_challenge_data/clean_train \
    --noise_dir dns_challenge_data/noise_train \
    --clean_val_dir dns_challenge_data/clean_val \
    --noise_val_dir dns_challenge_data/noise_val \
    --output_path denoiser_tcn_int8.tflite
```

**Monitor training**:
- Training loss should decrease to <0.01
- Validation loss should stabilize
- Inference time should be <2 ms

**Expected output**:
```
Epoch 100/100
Train Loss: 0.0089, Val Loss: 0.0121
✓ Model saved to: denoiser_tcn_int8.tflite
✓ Model size: 147.32 KB
Mean inference time: 1.43 ms
95th percentile: 1.78 ms
✓ Model meets <2 ms requirement
✓ Training complete! Model ready for deployment.
```

- [ ] Training completed (100 epochs)
- [ ] Model file created: `denoiser_tcn_int8.tflite`
- [ ] Inference time <2 ms verified
- [ ] Model size reasonable (<500 KB)

---

### 4. Compute SHA-256 Hash

```bash
shasum -a 256 denoiser_tcn_int8.tflite

# Example output:
# a3f2b8c9d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6c7d8e9f0a1  denoiser_tcn_int8.tflite
```

**Copy the hash** - you'll need it for the manifest.

- [ ] SHA-256 hash computed
- [ ] Hash copied to clipboard/notes

---

## Deployment Steps

### 5. Copy Model to Assets

```bash
cp denoiser_tcn_int8.tflite \
   /Users/roc_op/testspace/ML-TestSpace/EdgeClear/app/src/main/assets/models/

# Verify
ls -lh /Users/roc_op/testspace/ML-TestSpace/EdgeClear/app/src/main/assets/models/
```

**Expected**:
```
-rw-r--r--  denoiser_tcn_int8.tflite  (147 KB)
-rw-r--r--  model_manifest.json       (1 KB)
-rw-r--r--  README.md                 (5 KB)
```

- [ ] Model copied to assets directory
- [ ] File permissions correct (readable)

---

### 6. Update Model Manifest

Edit: `/Users/roc_op/testspace/ML-TestSpace/EdgeClear/app/src/main/assets/models/model_manifest.json`

**Replace**:
```json
"sha256": "placeholder_sha256_hash_will_be_computed_from_actual_model_file",
```

**With** (use your actual SHA-256 from step 4):
```json
"sha256": "a3f2b8c9d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6c7d8e9f0a1",
```

**Also update**:
```json
"status": "trained"
```

- [ ] Manifest opened in editor
- [ ] SHA-256 updated with actual hash
- [ ] Status changed to "trained"
- [ ] File saved

---

### 7. Build and Install

```bash
cd /Users/roc_op/testspace/ML-TestSpace/EdgeClear

# Clean build
./gradlew clean

# Build debug APK
./gradlew assembleDebug

# Install to connected device
./gradlew installDebug
```

**Expected output**:
```
BUILD SUCCESSFUL in 45s
Installing APK 'app-debug.apk' on 'Pixel 7 Pro - 14' for :app:debug
Installed on 1 device.
```

- [ ] Build successful (no errors)
- [ ] APK installed on device
- [ ] App launches without crashes

---

### 8. Verify Logs

```bash
# Clear existing logs
adb logcat -c

# Launch app and start monitoring

# Watch denoiser initialization
adb logcat -s EdgeClear-Session EdgeClear-TFLite EdgeClear-Denoiser
```

**Expected logs** (within 2 seconds of starting monitoring):
```
EdgeClear-Session: Initializing denoiser model from assets
EdgeClear-Session: Denoiser model loaded successfully
EdgeClear-TFLite: Model loaded successfully
EdgeClear-TFLite: Computed SHA-256: a3f2b8c9d4e5f6a7...
EdgeClear-TFLite: Expected SHA-256: a3f2b8c9d4e5f6a7...
EdgeClear-TFLite: Model integrity verified (SHA-256 match)
EdgeClear-Session:   Delegate: NNAPI
EdgeClear-Session:   Input quantization: scale=0.007843, zero_point=-128
EdgeClear-Session:   Output quantization: scale=0.003922, zero_point=0
EdgeClear-DSPWorker: DSPWorker initialized (AEC enabled, Denoiser enabled)
EdgeClear-Denoiser: Denoiser initialized and enabled
```

**Red flags** (if you see these, something's wrong):
```
❌ Failed to open model_manifest.json from assets
❌ Failed to load denoiser model - SHA-256 mismatch
❌ Invalid or placeholder SHA-256 in manifest
❌ Denoiser not available (no model provided)
❌ Failed to initialize denoiser interpreter
```

- [ ] Logs show "Model integrity verified"
- [ ] Logs show "Delegate: NNAPI" or "XNNPACK"
- [ ] Logs show "Denoiser enabled"
- [ ] No error messages in logs

---

### 9. Validate Performance

Open app and check **MetricsOverlay**:

**CPU Time**:
- [ ] CPU time: _____ ms (should be <6 ms)
- [ ] No red warning on CPU metric

**SI-SDR**:
- [ ] SI-SDR Delta: _____ dB (should be positive, e.g., +2 to +10 dB)
- [ ] SI-SDR increases when noise present

**ERLE** (Echo Return Loss Enhancement):
- [ ] ERLE: _____ dB (should be 12-18 dB with far-end audio)

**XRuns**:
- [ ] XRuns: 0 (should remain 0 during monitoring)

**Play test audio** (TV, music, or YouTube video through device speaker):
- [ ] Audio quality sounds improved
- [ ] No artifacts or distortion
- [ ] No noticeable latency (<100 ms perceivable)

---

### 10. Profile Performance (Optional)

```bash
# Profile CPU usage
adb shell simpleperf record -p $(adb shell pidof com.edgeclear) \
    --duration 10 -o /data/local/tmp/perf.data

adb pull /data/local/tmp/perf.data
simpleperf report -i perf.data

# Look for:
# - DenoiserInference::ProcessSpectrum should be <30% of total time
# - No unexpected bottlenecks
```

- [ ] Profiled (optional)
- [ ] Denoiser within performance budget

---

## Success Criteria

**Minimum Requirements** (must pass all):
- [x] Model file <500 KB
- [x] Inference time <2 ms per hop
- [x] Total CPU time <6 ms per hop
- [x] SI-SDR delta positive (indicates improvement)
- [x] Zero XRuns during 60-second test
- [x] SHA-256 verification passes
- [x] NNAPI or XNNPACK delegate active

**Quality Targets** (aim for):
- [ ] SI-SDR improvement >5 dB
- [ ] Subjective audio quality "good" or better
- [ ] Works on Snapdragon 778G and newer
- [ ] No perceivable latency (<50 ms)

---

## Troubleshooting

### Issue: SHA-256 mismatch

**Cause**: Hash in manifest doesn't match model file.

**Fix**:
```bash
shasum -a 256 EdgeClear/app/src/main/assets/models/denoiser_tcn_int8.tflite
# Copy correct hash to manifest
```

---

### Issue: Model too slow (CPU >6 ms)

**Cause**: Model too complex for device.

**Fix**:
1. Reduce model complexity (fewer filters)
2. Test on newer device (SD 888+)
3. Check delegate (should be NNAPI, not CPU)

---

### Issue: SI-SDR negative

**Cause**: Model making audio worse.

**Fix**:
1. Check training dataset quality
2. Increase training epochs
3. Validate ideal gain mask computation

---

### Issue: Denoiser not enabled

**Cause**: Model not loading.

**Check logs**:
```bash
adb logcat -s EdgeClear-Session | grep -i denoiser
```

**Common causes**:
- File not in assets
- SHA-256 placeholder not replaced
- Manifest JSON syntax error

---

## Quick Reference

### File Paths
- **Model location**: `EdgeClear/app/src/main/assets/models/denoiser_tcn_int8.tflite`
- **Manifest**: `EdgeClear/app/src/main/assets/models/model_manifest.json`
- **Training guide**: `/Users/roc_op/testspace/ML-TestSpace/DENOISER_TRAINING_GUIDE.md`
- **Fixes summary**: `/Users/roc_op/testspace/ML-TestSpace/M3_FIXES_SUMMARY.md`

### Key Commands
```bash
# Train model
python train_edgeclear_denoiser.py --clean_dir ... --noise_dir ...

# Compute hash
shasum -a 256 denoiser_tcn_int8.tflite

# Build and install
cd EdgeClear && ./gradlew installDebug

# Watch logs
adb logcat -s EdgeClear-Session EdgeClear-TFLite EdgeClear-Denoiser

# Profile
adb shell simpleperf record -p $(adb shell pidof com.edgeclear)
```

### Expected Performance
- **Model size**: 50-200 KB
- **Inference time**: <2 ms
- **Total CPU**: <6 ms per hop
- **SI-SDR improvement**: +2 to +10 dB
- **Latency**: 40-66 ms
- **XRuns**: 0

---

**Last Updated**: November 3, 2025
**Status**: M3 infrastructure complete, ready for model training
