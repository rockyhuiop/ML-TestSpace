# Data Model: EdgeClear Audio Enhancement

**Feature**: 001-edgeclear-av-vad
**Date**: 2025-11-03

## Overview

This document defines the core data entities, their fields, relationships, validation rules, and state machines for the EdgeClear audio enhancement system. Entities are organized by layer: **DSP Core** (C++ real-time processing), **Application State** (Kotlin UI/ViewModel), and **Persistence** (recordings + metadata).

---

## Entity Catalog

| Entity | Layer | Purpose | Lifetime |
|--------|-------|---------|----------|
| `AudioSession` | Application | Current monitoring session state | Active session only |
| `MetricsSnapshot` | Application | Real-time metrics at given timestamp | 1-second snapshots, circular buffer |
| `ABRecording` | Persistence | Metadata for A/B WAV file set | Permanent (until user deletes) |
| `ProcessingPreset` | Application | Configuration bundle for presets | Static (loaded at init) |
| `AVVADState` | Application | Audio-visual VAD status | Active session only |
| `ComponentState` | Application | Enable/disable state of DSP components | Active session only |
| `FarEndClip` | Persistence | Far-end reference audio metadata | Static (bundled in APK) |
| `DSPFrameBuffer` | DSP Core | Single audio frame (160 samples @ 16 kHz) | Transient (ring buffer slot) |
| `STFTState` | DSP Core | STFT analysis/synthesis state | Active session only |
| `AECFilterState` | DSP Core | AEC adaptive filter coefficients + history | Active session only |
| `DenoiserModelState` | DSP Core | TFLite interpreter + tensor buffers | Loaded at init, reused |

---

## Application Layer Entities

### AudioSession

**Purpose**: Represents a single monitoring session with accumulated state and configuration.

**Lifecycle**: Created when user presses "Start Monitoring", destroyed on "Stop Monitoring" or app backgrounding.

**Fields**:

| Field | Type | Validation | Description |
|-------|------|------------|-------------|
| `sessionId` | UUID | Non-null, unique | Random UUID generated on session start |
| `startTime` | Instant | Non-null | Timestamp when monitoring started (ISO 8601) |
| `duration` | Duration | ≥0 | Elapsed time since start (updated every second) |
| `xrunCount` | Int | ≥0 | Accumulated buffer underflow/overflow count |
| `currentPreset` | ProcessingPreset | Non-null | Active preset (Low-latency / Quality / Battery saver) |
| `componentState` | ComponentState | Non-null | Enable/disable flags for AEC/RES/denoiser/AV-VAD |
| `isRecording` | Boolean | - | True if A/B recording in progress |
| `recordingStartTime` | Instant? | Null if not recording | Timestamp when recording started |
| `recordingDuration` | Int | 10-30 seconds | User-selected recording duration (seconds) |

**Relationships**:
- `1:N` with `MetricsSnapshot` (session has many snapshots)
- `1:N` with `ABRecording` (session can produce multiple recordings)

**State Machine**:

```text
[Idle] --Start Monitoring--> [Monitoring]
[Monitoring] --Start Recording--> [MonitoringAndRecording]
[MonitoringAndRecording] --Stop Recording--> [Monitoring]
[MonitoringAndRecording] --Recording Complete (auto)--> [Monitoring]
[Monitoring] --Stop Monitoring--> [Idle]
[MonitoringAndRecording] --Stop Monitoring--> [Idle] (aborts recording)
```

**Validation Rules**:
- `START_MONITORING`: Requires RECORD_AUDIO permission granted.
- `START_RECORDING`: Cannot start if `isRecording == true`. Must check storage availability (≥15 MB for 30 sec).
- `STOP_MONITORING`: If `isRecording`, prompt user "Recording in progress. Stop anyway?"

---

### MetricsSnapshot

**Purpose**: Captures real-time DSP metrics at a given timestamp for display in metrics overlay.

**Lifecycle**: Created every 1 second while monitoring active. Stored in circular buffer (last 60 snapshots = 1 minute history).

**Fields**:

| Field | Type | Validation | Description |
|-------|------|------------|-------------|
| `timestamp` | Instant | Non-null | Time of snapshot (ISO 8601) |
| `erle_dB` | Float | 0.0-30.0 | Echo Return Loss Enhancement in dB |
| `siSdr_dB` | Float | -10.0 to +20.0 | SI-SDR delta vs raw input (estimated) |
| `cpuMs` | Float | 0.0-10.0 | CPU time per 10 ms hop in milliseconds |
| `latencyMs` | Float | 0.0-100.0 | End-to-end latency in milliseconds |
| `xrunCount` | Int | ≥0 | Cumulative XRun count since session start |
| `vadState` | VadState | Enum | Voice Activity Detection state |
| `dtdState` | DtdState | Enum | Double-Talk Detection state |
| `avVadMode` | String | "active" / "fallback" / "disabled" | AV-VAD status |

**Enums**:

```kotlin
enum class VadState {
    INACTIVE,  // No speech detected
    ACTIVE     // Speech detected
}

enum class DtdState {
    SILENCE,       // No near-end or far-end audio
    NEAR_END_ONLY, // Near-end speech only
    FAR_END_ONLY,  // Far-end audio only
    DOUBLE_TALK    // Both near-end speech and far-end audio
}
```

**Validation Rules**:
- `erle_dB`: Clamp to [0, 30] (negative ERLE nonsensical, >30 dB rare).
- `cpuMs`: Clamp to [0, 10] (display as red if >6 ms to indicate budget violation).
- `latencyMs`: Clamp to [0, 100] (display as red if >40 ms).
- Display granularity: 0.1 dB (ERLE), 0.1 ms (CPU/latency).

---

### ABRecording

**Purpose**: Metadata for a set of three synchronized A/B WAV files (raw, far-end, enhanced).

**Lifecycle**: Created when recording completes. Persists until user deletes via Record screen library manager.

**Fields**:

| Field | Type | Validation | Description |
|-------|------|------------|-------------|
| `recordingId` | UUID | Non-null, unique | Random UUID |
| `timestamp` | Instant | Non-null | Recording start time (ISO 8601) |
| `duration` | Int | 10-30 | Recording duration in seconds |
| `sampleRate` | Int | 48000 | Always 48 kHz |
| `channels` | Int | 1 or 2 | Mono or stereo (device-dependent) |
| `rawMicPath` | String | Non-null, valid file path | Path to raw microphone WAV |
| `farEndPath` | String | Non-null, valid file path | Path to far-end reference WAV |
| `enhancedPath` | String | Non-null, valid file path | Path to enhanced output WAV |
| `metadataPath` | String | Non-null, valid file path | Path to JSON metadata file |
| `sessionConfig` | SessionConfig | Non-null | Snapshot of session config at recording time |

**SessionConfig** (embedded struct):

| Field | Type | Description |
|-------|------|-------------|
| `preset` | String | "Low-latency" / "Quality" / "Battery saver" |
| `aecEnabled` | Boolean | AEC on/off |
| `resEnabled` | Boolean | RES on/off |
| `denoiserEnabled` | Boolean | Denoiser on/off |
| `avVadEnabled` | Boolean | AV-VAD on/off (if available) |

**Relationships**:
- `N:1` with `AudioSession` (many recordings belong to one session)

**File Naming Convention**:
```
edgeclear_YYYYMMDD_HHMMSS_raw.wav
edgeclear_YYYYMMDD_HHMMSS_far.wav
edgeclear_YYYYMMDD_HHMMSS_enhanced.wav
edgeclear_YYYYMMDD_HHMMSS_metadata.json
```

**Metadata JSON Schema**:
```json
{
  "recordingId": "uuid-string",
  "timestamp": "2025-11-03T14:30:00Z",
  "duration": 15,
  "sampleRate": 48000,
  "channels": 1,
  "sessionConfig": {
    "preset": "Quality",
    "aecEnabled": true,
    "resEnabled": true,
    "denoiserEnabled": true,
    "avVadEnabled": false
  },
  "deviceInfo": {
    "model": "SM-G998B",  // Redacted in logs, included in metadata
    "osVersion": "Android 13",
    "ndkVersion": "r26"
  }
}
```

**Validation Rules**:
- `duration`: Must match WAV file duration (within ±1 second).
- `sampleRate`: Always 48000 (Constitution int16 I/O requirement).
- `rawMicPath`, `farEndPath`, `enhancedPath`: Files must exist and be time-aligned within ±1 sample (20.8 μs).
- `metadataPath`: JSON must be valid and parseable.

---

### ProcessingPreset

**Purpose**: Configuration bundle defining DSP parameters for each preset (Low-latency, Quality, Battery saver).

**Lifecycle**: Loaded once at app initialization, immutable at runtime.

**Fields**:

| Field | Type | Validation | Description |
|-------|------|------------|-------------|
| `name` | String | Non-null, unique | "Low-latency" / "Quality" / "Battery saver" |
| `targetLatencyMs` | Int | 20-40 | Target end-to-end latency in milliseconds |
| `bufferHops` | Int | 1-6 | Number of hops to buffer in SPSC queues (1 = tight, 6 = safe) |
| `aecFilterLength` | Int | 4-16 | Number of AEC partitions (4 = 85 ms, 8 = 170 ms, 16 = 340 ms) |
| `denoiserFrameRate` | Int | 1 or 2 | Process every Nth hop (1 = every hop, 2 = every other hop) |
| `resEnabled` | Boolean | - | Enable/disable RES post-filter |
| `cameraFrameRateHz` | Int | 10-30 | Camera frame rate for AV-VAD (lower = less CPU) |

**Preset Definitions**:

| Preset | targetLatencyMs | bufferHops | aecFilterLength | denoiserFrameRate | resEnabled | cameraFrameRateHz |
|--------|-----------------|------------|-----------------|-------------------|------------|-------------------|
| **Low-latency** | 25 | 1 | 4 | 1 | false | 15 |
| **Quality** | 38 | 2 | 8 | 1 | true | 30 |
| **Battery saver** | 35 | 2 | 8 | 2 | false | 10 |

**Validation Rules**:
- `targetLatencyMs`: Must be achievable with given `bufferHops` and DSP budget (validate on init).
- `aecFilterLength`: Powers of 2 preferred (4, 8, 16) for efficient partitioned convolution.
- `denoiserFrameRate`: Only 1 or 2 supported (higher values degrade quality unacceptably).

---

### ComponentState

**Purpose**: Enable/disable flags for each DSP component (AEC, RES, denoiser, AV-VAD).

**Lifecycle**: Created with session, updated via user toggles. Locked during A/B recording (FR-019d).

**Fields**:

| Field | Type | Validation | Description |
|-------|------|------------|-------------|
| `aecEnabled` | Boolean | - | Acoustic Echo Cancellation on/off |
| `resEnabled` | Boolean | - | Residual Echo Suppressor on/off |
| `denoiserEnabled` | Boolean | - | ML denoiser on/off |
| `avVadEnabled` | Boolean | - | Audio-Visual VAD on/off (if camera available) |
| `isLocked` | Boolean | - | True if A/B recording in progress (disables toggles) |

**State Machine**:

```text
[Unlocked] --Start Recording--> [Locked]
[Locked] --Stop Recording--> [Unlocked]
```

**Validation Rules**:
- `TOGGLE_COMPONENT`: Rejected if `isLocked == true`. Display toast "Cannot change components during recording".
- `avVadEnabled`: Can only be set to `true` if camera permission granted and face detection available. Graceful fallback to `false` if camera fails.

---

### AVVADState

**Purpose**: State of audio-visual VAD feature (camera availability, face detection, motion metric).

**Lifecycle**: Created with session (if AV-VAD enabled), updated every camera frame (10-30 Hz).

**Fields**:

| Field | Type | Validation | Description |
|-------|------|------------|-------------|
| `cameraAvailable` | Boolean | - | True if camera permission granted and camera accessible |
| `faceDetected` | Boolean | - | True if face detected in current frame |
| `faceConfidence` | Float | 0.0-1.0 | Face detection confidence (0 = no face, 1 = high confidence) |
| `mouthRoiBounds` | Rect? | Null if no face | Bounding box of mouth region (x, y, width, height) |
| `motionMetric` | Float | 0.0-1.0 | Normalized mouth motion magnitude (0 = no motion, 1 = high motion) |
| `fusionMode` | FusionMode | Enum | AV-VAD fusion state |
| `lastCameraFrameTime` | Instant | Non-null | Timestamp of last processed camera frame |

**FusionMode** (enum):
```kotlin
enum class FusionMode {
    DISABLED,       // AV-VAD feature off (camera permission denied or user disabled)
    ACTIVE,         // Camera working, face detected, fusing audio + visual VAD
    FALLBACK_NO_FACE,    // Camera working but no face detected → audio-only VAD
    FALLBACK_LOW_CONF,   // Face detected but confidence <70% → audio-only VAD
    FALLBACK_CAMERA_FAIL // Camera failed (error or occlusion) → audio-only VAD
}
```

**Validation Rules**:
- `faceConfidence`: If <0.7, switch to FALLBACK_LOW_CONF.
- `mouthRoiBounds`: Must be within frame bounds (0,0) to (width, height).
- `motionMetric`: Computed as optical flow magnitude in mouth ROI, low-pass filtered (500 ms window).
- `lastCameraFrameTime`: If stale (>200 ms), trigger FALLBACK_CAMERA_FAIL.

---

### FarEndClip

**Purpose**: Metadata for built-in far-end reference test audio clips bundled in APK.

**Lifecycle**: Static, loaded from `assets/far_end_clips/` on app init.

**Fields**:

| Field | Type | Validation | Description |
|-------|------|------------|-------------|
| `clipId` | String | Non-null, unique | Filename without extension (e.g., "music_01") |
| `displayName` | String | Non-null | Human-readable name (e.g., "Music: Classical Piano") |
| `category` | ClipCategory | Enum | Music / Speech / Silence |
| `filePath` | String | Non-null, valid asset path | Path in APK assets (e.g., "far_end_clips/music_01.wav") |
| `duration` | Int | >0 | Clip duration in seconds |
| `sampleRate` | Int | 48000 | Always 48 kHz (Constitution I/O requirement) |
| `description` | String | Non-null | Brief description (e.g., "Mozart Piano Concerto excerpt") |

**ClipCategory** (enum):
```kotlin
enum class ClipCategory {
    MUSIC,
    SPEECH,
    SILENCE
}
```

**Bundled Clips**:

| clipId | displayName | category | duration | description |
|--------|-------------|----------|----------|-------------|
| `music_01` | "Classical Piano" | MUSIC | 30 sec | Mozart Piano Concerto No. 21 |
| `music_02` | "Rock Drums" | MUSIC | 30 sec | Heavy drum + bass mix (hard AEC case) |
| `speech_far_01` | "Male Speech" | SPEECH | 30 sec | Clean male speech, neutral accent |
| `speech_far_02` | "Female Speech" | SPEECH | 30 sec | Clean female speech, neutral accent |
| `silence` | "Silence" | SILENCE | 30 sec | Digital silence (all zeros) for calibration |

**Validation Rules**:
- `filePath`: File must exist in APK assets and be readable.
- `sampleRate`: Must be 48000 Hz (validate on load, reject otherwise).
- `duration`: Must match actual WAV file duration (within ±1 second).

---

## DSP Core Entities

### DSPFrameBuffer

**Purpose**: Single audio frame (160 samples @ 16 kHz, one 10 ms hop) passed between RT callback and DSP worker thread.

**Lifecycle**: Transient, reused in ring buffer slots (4-6 hops capacity).

**Fields**:

| Field | Type | Validation | Description |
|-------|------|------------|-------------|
| `samples` | `int16_t[160]` | -16383 to +16383 | Audio samples (3 dB headroom reserved) |
| `timestamp` | `uint64_t` | Monotonic, ≥0 | Sample count since session start (for alignment) |
| `isClipping` | `bool` | - | True if any sample == ±16384 (clipping detected) |

**Validation Rules**:
- `samples`: Clamp to [-16383, +16383] on input to reserve 3 dB headroom (Constitution I/O requirement).
- `timestamp`: Strictly increasing across frames (validate in ring buffer push).

---

### STFTState

**Purpose**: STFT analysis/synthesis state (window buffer, overlap-add buffer, FFT plan).

**Lifecycle**: Allocated once on session start, reused across frames, deallocated on session stop.

**Fields**:

| Field | Type | Description |
|-------|------|-------------|
| `windowLUT` | `float[512]` | Pre-computed sqrt-Hann window |
| `analysisBuffer` | `float[512]` | Input buffer for STFT analysis (512 samples, 50% overlap) |
| `synthesisBuffer` | `float[512]` | Overlap-add buffer for ISTFT synthesis |
| `fftPlan` | `pffft_setup_t*` | pffft plan for 512-pt complex FFT |
| `workBuffer` | `float[512]` | Scratch buffer for pffft (aligned, 16-byte) |

**Validation Rules**:
- `windowLUT`: Validate COLA condition on initialization: `window[i]² + window[i+256]² == 1.0 ± 1e-6`.
- `analysisBuffer`, `synthesisBuffer`: Zero-initialized on session start.
- `fftPlan`, `workBuffer`: Pre-allocated during init, never freed/reallocated during session (zero-allocation rule).

---

### AECFilterState

**Purpose**: AEC adaptive filter state (partitioned-block FD-NLMS coefficients + history buffers).

**Lifecycle**: Allocated once on session start, updated every hop, deallocated on session stop.

**Fields**:

| Field | Type | Description |
|-------|------|-------------|
| `numPartitions` | `int` | 8 (configurable via preset, 4-16 range) |
| `partitionSize` | `int` | 1024 complex samples per partition |
| `filterCoeffs` | `complex<float>[8][1024]` | Frequency-domain filter coefficients (8 partitions × 1024 bins) |
| `nearHistory` | `complex<float>[8][1024]` | Near-end signal history (for partitioned convolution) |
| `farHistory` | `complex<float>[8][1024]` | Far-end signal history |
| `stepSize` | `float` | μ = 0.3 (FD-NLMS adaptation rate) |
| `epsilon` | `float` | ε = 1e-6 (power normalization regularization) |
| `dtdHangover` | `int` | 5 hops (50 ms hangover counter for DTD) |

**Validation Rules**:
- `filterCoeffs`: Initialize to zero on session start (cold start).
- `nearHistory`, `farHistory`: Circular buffers, oldest partition discarded when full.
- `stepSize`: Clamp to [0.1, 0.5] (too low = slow adaptation, too high = instability).
- `dtdHangover`: Decrement each hop during double-talk, reset to 5 when double-talk ends.

---

### DenoiserModelState

**Purpose**: TFLite interpreter state for INT8 QAT denoiser model.

**Lifecycle**: Loaded once on app init, reused across all sessions, deallocated on app termination.

**Fields**:

| Field | Type | Description |
|-------|------|-------------|
| `interpreter` | `tflite::Interpreter*` | TFLite interpreter (model loaded from assets) |
| `inputTensor` | `TfLiteTensor*` | Pointer to input tensor (256 bins × 1 frame, INT8) |
| `outputTensor` | `TfLiteTensor*` | Pointer to output tensor (256 bins × 1 frame, INT8) |
| `delegate` | `TfLiteDelegate*` | NNAPI delegate (or nullptr if CPU fallback) |
| `modelSHA256` | `std::string` | SHA-256 hash of loaded model (for integrity check) |
| `inputScale` | `float` | Quantization scale for input tensor |
| `inputZeroPoint` | `int` | Quantization zero-point for input tensor |
| `outputScale` | `float` | Quantization scale for output tensor |
| `outputZeroPoint` | `int` | Quantization zero-point for output tensor |

**Validation Rules**:
- `modelSHA256`: Verify against `assets/models/model_manifest.json` on load. Reject if mismatch.
- `inputScale`, `outputScale`: Extract from TFLite model metadata on load.
- `interpreter->AllocateTensors()`: Must succeed on init, otherwise fail gracefully (disable denoiser, log error).

---

## Relationships Diagram

```text
AudioSession (1) ----< (N) MetricsSnapshot
             |
             +--------< (N) ABRecording
             |
             +-------- (1) ComponentState
             |
             +-------- (1) AVVADState (if AV-VAD enabled)

FarEndClip (static, loaded at init)

DSPFrameBuffer --[SPSC Queue]--> STFTState --> AECFilterState --> DenoiserModelState
```

---

## Validation Summary

| Entity | Key Validation Rules |
|--------|----------------------|
| `AudioSession` | RECORD_AUDIO permission required; storage check before recording; prompt on stop if recording active |
| `MetricsSnapshot` | Clamp ERLE [0, 30], CPU [0, 10], latency [0, 100]; display red if CPU >6 ms or latency >40 ms |
| `ABRecording` | WAV files time-aligned ±1 sample; duration matches metadata ±1 second; JSON valid |
| `ProcessingPreset` | Target latency achievable with buffer hops + CPU budget; immutable at runtime |
| `ComponentState` | Toggles locked during recording; avVadEnabled only if camera available |
| `AVVADState` | Face confidence <0.7 triggers fallback; stale camera frame >200 ms triggers fallback |
| `FarEndClip` | File exists in assets; sample rate == 48 kHz; duration matches WAV |
| `DSPFrameBuffer` | Samples clamped to ±16383 (3 dB headroom); timestamp strictly increasing |
| `STFTState` | COLA validated <0.5 LSB on init; buffers pre-allocated (zero-allocation rule) |
| `AECFilterState` | Step size clamped [0.1, 0.5]; history buffers circular; coefficients zero-init |
| `DenoiserModelState` | Model SHA-256 verified on load; input/output tensors allocated successfully |

---

## Persistence Strategy

**Recordings** (app-private storage, Android scoped storage):
- Location: `/data/data/com.edgeclear/files/recordings/`
- Format: WAV (48 kHz, 16-bit PCM) + JSON metadata
- Retention: User-managed (delete via Record screen library manager)
- Backup: Not backed up to cloud (privacy requirement)

**Far-End Clips** (APK assets, read-only):
- Location: `assets/far_end_clips/`
- Format: WAV (48 kHz, 16-bit PCM)
- Immutable: Bundled with APK, cannot be modified at runtime

**Models** (APK assets, read-only):
- Location: `assets/models/`
- Format: TFLite (INT8 QAT)
- Integrity: SHA-256 verified on load (reject if mismatch)

---

## Next Steps

This data model feeds into:
1. **JNI API contract** (`contracts/jni-api.md`): Defines Kotlin ↔ C++ data marshalling.
2. **DSP pipeline contract** (`contracts/dsp-pipeline.md`): Defines C++ module interfaces.
3. **UI state contract** (`contracts/ui-state.md`): Defines ViewModel → UI data flow.
4. **Task generation** (`/speckit.tasks`): Tasks for implementing each entity + validation logic.
