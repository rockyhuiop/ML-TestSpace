# Feature Specification: EdgeClear Audio Enhancement with AV-VAD Gate

**Feature Branch**: `001-edgeclear-av-vad`
**Created**: 2025-11-03
**Status**: Draft
**Input**: Real-time Android app that removes echo and noise during hands-free calls and live recording, with a metrics overlay and A/B recording. Optional A+ adds a lip-aware VAD gate using front-camera mouth motion to stabilize suppression during TV/music interference.

## Clarifications

### Session 2025-11-03

- Q: How should monitoring and recording functionality be organized across the two screens? → A: Monitor screen handles real-time metrics + controls (toggles, presets, audio playback). Record screen handles recording initiation, playback of saved A/B files, and recordings library management.
- Q: What should happen when a user attempts to record but available storage is insufficient? → A: Block recording attempt, display error message "Insufficient storage. Need X MB free." Allow recording only after user frees space.
- Q: Should real-time metrics remain visible and updating while A/B recording is in progress? → A: Yes - metrics overlay continues updating during recording. Users can observe ERLE, CPU, latency, VAD/DTD states in real-time while recording progresses.
- Q: Can users toggle component states (AEC/RES/denoiser/AV-VAD) while A/B recording is actively in progress? → A: No - component toggle controls are locked/disabled during recording. Users must stop recording to change components. This ensures recording metadata accurately reflects the entire recording.
- Q: How should users select and control far-end reference audio playback? → A: Built-in library of categorized test clips (music, speech, silence) with dropdown/list selector. Playback controls: play/pause/stop, loop toggle, volume slider. Support import custom WAV files for testing.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Real-Time Audio Enhancement Monitoring (Priority: P1)

A communications app developer is testing their hands-free calling feature on a mid-tier Snapdragon device. They need to see real-time metrics showing how well echo cancellation and noise removal are performing, including latency measurements and processing costs.

**Why this priority**: This is the core value proposition - real-time audio enhancement with observable quality metrics. Without this, the product has no demonstrable value.

**Independent Test**: Launch the app, enable microphone capture with far-end audio playback (can use pre-recorded far-end reference), and verify the metrics overlay displays live ERLE, latency, CPU usage, and audio quality indicators. Enhanced audio output should be clearly improved compared to raw microphone input.

**Acceptance Scenarios**:

1. **Given** the app is launched on an Android 10+ device with microphone permission granted, **When** the user starts monitoring mode, **Then** the system captures microphone audio, processes it through AEC and denoiser, renders enhanced output, and displays real-time metrics (ERLE, SI-SDR delta estimate, CPU ms/hop, end-to-end latency, XRun count, VAD/DTD states) updating every second.

2. **Given** monitoring is active with far-end music playback, **When** the user speaks while music plays (double-talk scenario), **Then** the metrics overlay shows DTD (Double-Talk Detection) state changes, ERLE values reflecting echo suppression performance, and enhanced output preserves near-end speech while suppressing far-end echo.

3. **Given** monitoring is running for 10 minutes continuously, **When** the user reviews the session, **Then** no audio dropouts occurred (XRun count = 0), CPU usage remained consistently below 6 ms per 10 ms hop, and end-to-end latency stayed at or below 40 ms.

---

### User Story 2 - A/B Audio Recording for Validation (Priority: P1)

A QA engineer needs to validate ERLE performance and audio quality improvements by recording synchronized raw and enhanced audio clips for offline analysis and comparison.

**Why this priority**: Without recorded evidence, it's impossible to validate quality improvements or debug issues. This is essential for professional use.

**Independent Test**: Start monitoring, enable A/B recording, capture 10-30 seconds of audio with far-end reference, stop recording, and verify three synchronized WAV files are saved (raw microphone, far-end reference, enhanced output). Playback should show clear audible improvement in enhanced audio.

**Acceptance Scenarios**:

1. **Given** monitoring is active, **When** the user initiates A/B recording for 15 seconds during a noisy caf� scenario with far-end speech, **Then** the system saves three synchronized 48 kHz WAV files: raw microphone input, far-end reference, and enhanced output.

2. **Given** A/B WAV files have been recorded, **When** the user plays back the raw microphone and enhanced output files, **Then** the enhanced output demonstrates audibly reduced background noise, suppressed echo during far-end playback, and preserved near-end speech clarity.

3. **Given** the user wants to validate against golden test clips, **When** they run the recorded enhanced output through objective metrics (PESQ, STOI, SI-SDR) compared to the raw input, **Then** the enhanced output shows +0.3 PESQ minimum, +0.02 STOI minimum, and +4 dB SI-SDR minimum improvement over the noisy baseline.

---

### User Story 3 - Performance Preset Optimization (Priority: P2)

A user in a living room with far-end music interference wants to balance audio quality, latency, and battery consumption by selecting an appropriate preset for their use case.

**Why this priority**: Different use cases have different constraints. Real-time calls prioritize low latency; recordings prioritize quality; extended sessions need battery efficiency. This is a quality-of-life feature that doesn't block core functionality.

**Independent Test**: Select each preset (Low-latency, Quality, Battery saver) and verify the system adjusts processing parameters accordingly, with observable changes in metrics (latency, CPU usage) and audio characteristics.

**Acceptance Scenarios**:

1. **Given** monitoring is active on "Quality" preset, **When** the user switches to "Low-latency" preset, **Then** the metrics overlay shows reduced end-to-end latency (approaching minimum possible, ~20-30 ms), potentially slightly higher CPU usage, and audio processing prioritizes speed over aggressive enhancement.

2. **Given** monitoring is on "Low-latency" preset with 25 ms latency, **When** the user switches to "Quality" preset, **Then** latency may increase slightly (up to 40 ms budget) while ERLE and SI-SDR improvements become more aggressive, providing maximum echo suppression and noise reduction.

3. **Given** monitoring is on "Quality" preset, **When** the user switches to "Battery saver" preset for an extended recording session, **Then** CPU usage per hop decreases (approaching lower bound), processing becomes more selective (e.g., VAD-gated denoiser activation), and battery drain is minimized while maintaining acceptable audio quality.

---

### User Story 4 - Component Toggle for Debugging (Priority: P2)

A communications app developer needs to isolate the contribution of each processing component (AEC, residual echo suppressor, denoiser, AV-VAD) by toggling them on/off individually.

**Why this priority**: Essential for debugging and understanding which component solves which problem. Helps developers tune integration and identify configuration issues. Not blocking for end-users but critical for professional adoption.

**Independent Test**: Toggle each component (AEC, RES, denoiser, AV-VAD) on and off during monitoring, and verify audio output and metrics reflect the presence/absence of that component's processing.

**Acceptance Scenarios**:

1. **Given** monitoring is active with all components enabled during far-end music playback, **When** the user disables AEC, **Then** ERLE metric drops to near-zero (no echo cancellation), enhanced output contains audible far-end echo, and denoiser still suppresses background noise on near-end speech.

2. **Given** AEC is enabled with residual echo still present, **When** the user enables the residual echo suppressor (RES), **Then** ERLE improves by additional 3-6 dB, remaining echo "tails" are suppressed, and metrics overlay indicates RES active state.

3. **Given** monitoring is active in a noisy caf� with AEC handling echo, **When** the user toggles the denoiser off, **Then** background noise (chatter, machine hum) becomes audible in enhanced output, SI-SDR delta drops significantly, but echo suppression (ERLE) remains unaffected.

4. **Given** monitoring is active with AV-VAD available (front camera permission granted), **When** the user disables AV-VAD, **Then** the system falls back to audio-only VAD, lip-motion cues are no longer fused, and VAD state may become less stable during TV/music interference scenarios.

---

### User Story 5 - AV-VAD Lip-Motion Fusion (A+) (Priority: P3)

A user experiencing frequent VAD false triggers from TV/music in the background wants to enable camera-based lip-motion detection to stabilize voice activity detection and reduce spurious denoiser activation.

**Why this priority**: This is an optional advanced feature (A+) that provides incremental value in specific challenging scenarios. The core product works without it. It's a differentiator but not essential for launch.

**Independent Test**: Enable AV-VAD with front camera permission granted, position face in view, and verify the system fuses mouth-region motion with audio VAD to produce more stable VAD state during TV/music interference. If camera is unavailable, feature gracefully disables without affecting core functionality.

**Acceptance Scenarios**:

1. **Given** the app has camera permission and the user's face is visible to the front camera, **When** the user enables AV-VAD and speaks while TV audio plays in the background, **Then** the VAD state indicator shows stable "speech active" during actual speech (mouth motion + audio energy) and remains "inactive" during TV-only audio (audio energy without mouth motion), reducing false positives compared to audio-only VAD.

2. **Given** AV-VAD is enabled but camera permission is denied or camera is unavailable, **When** the user starts monitoring, **Then** the system gracefully disables AV-VAD, displays a notification indicating "AV-VAD unavailable, using audio-only VAD", and continues full operation with audio-only VAD without crashes or degraded core functionality.

3. **Given** AV-VAD is active with camera enabled, **When** the user moves out of camera view or covers the camera, **Then** the system detects mouth ROI (Region of Interest) loss, falls back to audio-only VAD temporarily, and automatically resumes AV-VAD fusion when face re-enters view, all without interrupting audio processing.

---

### Edge Cases

- **What happens when the device has only a single microphone?** System operates with single-channel AEC and denoiser. Multi-mic beamforming features (if any) are disabled. Core functionality remains operational with slightly reduced spatial noise suppression.

- **What happens when audio input is clipping (overdriven microphone)?** System detects clipping (samples at max int16 range), logs a warning in metrics overlay, applies soft limiting to prevent internal overflow, and continues processing. A/B recordings capture clipping state for offline analysis.

- **What happens when far-end reference is silent for extended periods?** AEC adaptive filter freezes or slowly decays, DTD indicates "no far-end" state, and denoiser continues operating on near-end input. No echo cancellation is needed when there's no echo source.

- **How does the system handle device hot-plug (headset/Bluetooth connect/disconnect)?** System detects audio device change via Android AudioManager callback, reinitializes audio I/O with new device configuration, flushes processing buffers to avoid stale state, and resumes processing within 100-200 ms. Brief audio gap is acceptable; no crash.

- **What happens when the app is backgrounded during monitoring?** System reduces processing to minimal state (audio stops or moves to foreground service depending on Android lifecycle policy), releases camera resources, and stops metrics updates. When foregrounded, user must restart monitoring session.

- **How does AV-VAD handle poor lighting or face occlusion (mask, hand)?** Mouth ROI detection fails or confidence drops. System falls back to audio-only VAD automatically. Metrics overlay indicates "AV-VAD: fallback" state. No degradation to audio quality.

- **What happens when multiple users are in front of the camera?** System selects the largest or most central detected face for mouth ROI tracking (implementation detail not specified here). If no clear primary face, uses audio-only VAD. A+ feature is single-user focused; multi-user is out of scope.

---

## Requirements *(mandatory)*

### Functional Requirements

**Audio Capture & Playback**

- **FR-001**: System MUST capture microphone input at 48 kHz sample rate, 16-bit PCM, mono or stereo depending on device microphone configuration.
- **FR-002**: System MUST play far-end reference audio (simulated or live) synchronized with microphone capture for AEC reference.
- **FR-002a**: System MUST provide a built-in library of categorized far-end reference test clips (music, speech, silence) accessible via dropdown/list selector.
- **FR-002b**: System MUST provide far-end playback controls: play/pause/stop, loop toggle (continuous playback for extended testing), and volume slider (0-100%).
- **FR-002c**: System MUST support importing custom WAV files (48 kHz, 16-bit PCM) as far-end reference for user-defined test scenarios.
- **FR-003**: System MUST render enhanced audio output in real-time (loopback to speaker or headset) with end-to-end latency d 40 ms.

**DSP Processing Pipeline**

- **FR-004**: System MUST implement Acoustic Echo Cancellation (AEC) using partitioned-block frequency-domain NLMS adaptive filtering to suppress far-end echo in microphone input.
- **FR-005**: System MUST implement a residual echo suppressor (RES) to attenuate remaining echo artifacts after AEC adaptive filtering.
- **FR-006**: System MUST implement a denoiser (spectral-domain or ML-based) to suppress background noise (stationary and non-stationary) while preserving speech quality.
- **FR-007**: System MUST implement double-talk detection (DTD) to identify when near-end speech and far-end audio occur simultaneously, preventing AEC filter divergence.
- **FR-008**: System MUST process audio in fixed 10 ms hops (160 samples @ 16 kHz internal processing rate, resampled from 48 kHz input) to meet latency budget.

**Real-Time Metrics Overlay**

- **FR-009**: System MUST display Echo Return Loss Enhancement (ERLE) in dB, updated every second, indicating AEC performance.
- **FR-010**: System MUST display estimated SI-SDR (Scale-Invariant Signal-to-Distortion Ratio) delta in dB, showing signal quality improvement over raw input.
- **FR-011**: System MUST display CPU processing time per 10 ms hop in milliseconds, indicating computational cost.
- **FR-012**: System MUST display end-to-end latency in milliseconds, from microphone capture to enhanced output render.
- **FR-013**: System MUST display XRun count (buffer underflows/overflows) accumulated since monitoring start. XRuns indicate real-time deadline misses.
- **FR-014**: System MUST display VAD (Voice Activity Detection) state (active/inactive) and DTD (Double-Talk Detection) state (near-end only, far-end only, double-talk, silence).
- **FR-014a**: Real-time metrics overlay MUST continue updating during A/B recording, allowing users to observe ERLE, CPU, latency, XRuns, and VAD/DTD states while recording is in progress.

**User Controls**

- **FR-015**: Users MUST be able to toggle AEC on/off independently.
- **FR-016**: Users MUST be able to toggle residual echo suppressor (RES) on/off independently.
- **FR-017**: Users MUST be able to toggle denoiser on/off independently.
- **FR-018**: Users MUST be able to toggle AV-VAD (audio-visual VAD) on/off independently when camera is available.
- **FR-019**: Users MUST be able to select one of three presets: "Low-latency" (minimize latency, lighter processing), "Quality" (maximize ERLE and SI-SDR, up to 40 ms latency budget), "Battery saver" (reduce CPU load, selective processing).
- **FR-019d**: While A/B recording is in progress, component toggle controls (AEC, RES, denoiser, AV-VAD) and preset selector MUST be locked/disabled. Users must stop recording to change component states or presets. This ensures recording metadata (session configuration) accurately reflects the entire recording period.

**UI Organization**

- **FR-019a**: System MUST provide a Monitor screen that displays real-time metrics overlay, component toggle controls (AEC, RES, denoiser, AV-VAD), preset selector, and audio playback controls for far-end reference.
- **FR-019b**: System MUST provide a Record screen that allows users to initiate A/B recording, playback previously saved A/B WAV files, and manage the recordings library (view, delete, share).
- **FR-019c**: Users MUST be able to navigate between Monitor and Record screens without interrupting active audio processing or losing current session state.

**A/B Recording**

- **FR-020**: Users MUST be able to initiate A/B recording for a configurable duration (10-30 seconds, user-selectable).
- **FR-021**: System MUST save three synchronized 48 kHz, 16-bit PCM WAV files: raw microphone input, far-end reference, and enhanced output.
- **FR-022**: WAV files MUST be time-aligned (same start time) to enable offline frame-by-frame comparison and metric computation.
- **FR-023**: System MUST store recordings in app-private storage (Android scoped storage) with timestamped filenames (e.g., `edgeclear_YYYYMMDD_HHMMSS_raw.wav`).
- **FR-023a**: Before initiating recording, system MUST check available storage space. If insufficient space for the selected recording duration (estimated: 15 MB for 30-second recording), system MUST block the recording attempt and display error message "Insufficient storage. Need X MB free." Recording is allowed only after user frees space via the recordings library manager.

**AV-VAD (A+) Audio-Visual Voice Activity Detection**

- **FR-024**: When AV-VAD is enabled and camera permission granted, system MUST detect mouth region (ROI) in front camera video frames.
- **FR-025**: System MUST compute motion metrics from mouth ROI (e.g., optical flow magnitude, frame differencing) to detect lip movement.
- **FR-026**: System MUST fuse mouth motion cues with audio-based VAD using late fusion (e.g., logical AND: speech active if both audio energy AND lip motion detected).
- **FR-027**: If camera is unavailable, denied, or face detection fails, system MUST gracefully fall back to audio-only VAD without affecting core audio processing.
- **FR-028**: AV-VAD processing MUST NOT increase end-to-end audio latency beyond the 40 ms budget (camera processing runs asynchronously; fusion uses recent motion state).

### Key Entities

- **AudioSession**: Represents a monitoring session with start time, duration, accumulated XRun count, current preset, and component enable states (AEC, RES, denoiser, AV-VAD).

- **MetricsSnapshot**: Captures real-time metrics at a given timestamp: ERLE (dB), SI-SDR delta (dB), CPU ms/hop, end-to-end latency (ms), VAD state, DTD state. Updated and displayed every second.

- **ABRecording**: Represents a set of three synchronized WAV files (raw, far-end, enhanced) with start timestamp, duration, sample rate, and file paths. Metadata includes session configuration (preset, enabled components) for traceability.

- **ProcessingPreset**: Configuration bundle defining parameter values for "Low-latency", "Quality", or "Battery saver" modes. Includes AEC filter length, denoiser aggressiveness, RES threshold, and VAD sensitivity.

- **AVVADState**: State of audio-visual VAD feature including camera availability, face detection status, mouth ROI bounding box, motion metric, and fusion mode (active/fallback).

---

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Enhanced audio output demonstrates +0.3 PESQ minimum, +0.02 STOI minimum, and +4 dB SI-SDR minimum improvement over raw noisy baseline when tested on golden clip suite (caf� noise, living room TV, far-end music).

- **SC-002**: System maintains zero XRuns (buffer underflows/overflows) during continuous 10-minute monitoring sessions on reference hardware (mid-tier Snapdragon device, Android 10+).

- **SC-003**: End-to-end latency from microphone capture to enhanced audio output remains at or below 40 ms under all presets and component configurations, verified via metrics overlay and external loopback measurement.

- **SC-004**: CPU processing time per 10 ms hop remains below 6 ms on reference hardware (leaving 4 ms margin), averaged over 1-minute monitoring window, across all presets.

- **SC-005**: AEC achieves 12-18 dB ERLE on standardized music far-end playback clips, measured via A/B recordings and offline analysis, demonstrating robust echo suppression.

- **SC-006**: A/B recording files are time-aligned within �1 sample (�20 �s @ 48 kHz), enabling frame-accurate offline comparison and metric computation.

- **SC-007**: QA engineers can complete a full validation workflow (start monitoring, toggle components, record A/B clips, review metrics, stop session) in under 5 minutes with no crashes or errors.

- **SC-008**: AV-VAD (when enabled with camera available) reduces VAD false positive rate by at least 30% compared to audio-only VAD in TV/music interference scenarios, measured on standardized test set.

- **SC-009**: When camera is unavailable or denied, app continues full operation with audio-only VAD with zero degradation to core audio processing quality or stability.

- **SC-010**: 90% of comms app developers report that the metrics overlay provides sufficient real-time feedback to diagnose audio quality issues during integration testing (qualitative user satisfaction survey).

---

## Assumptions

1. **Sample Rate**: Input audio is captured at 48 kHz (Android hardware standard) and downsampled to 16 kHz internally for DSP processing to meet latency and CPU budgets. Output is upsampled back to 48 kHz for rendering.

2. **Far-End Reference Source**: For testing and demo purposes, the app provides a built-in library of far-end reference clips (music, speech) that can be played through the speaker to simulate hands-free call scenarios. Integration with live VoIP call signaling is out of scope.

3. **Microphone Configuration**: System prefers stereo or multi-mic capture when available but operates fully on single-channel (mono) input. Multi-mic beamforming is optional and not a core requirement.

4. **Storage Permissions**: A/B recording WAV files are stored in app-private storage; no external storage write permission required (Android scoped storage model). Users can share recordings via Android share sheet if needed.

5. **AV-VAD Face Detection**: Mouth ROI detection uses on-device face detection (e.g., Android ML Kit or custom lightweight model). Detection latency < 50 ms per frame is acceptable since VAD fusion tolerates ~100 ms staleness (audio lookahead).

6. **Battery Saver Preset**: Reduces CPU by selectively activating denoiser only when VAD detects speech, using lighter AEC filter, and lowering camera frame rate for AV-VAD. Exact parameter tuning is implementation-specific.

7. **Metrics Update Rate**: Real-time metrics overlay updates every 1 second (not per-hop) to balance UI responsiveness and computational cost. Per-hop metrics are logged internally for detailed debugging.

8. **Golden Clip Suite**: Acceptance testing assumes availability of a predefined set of standardized audio test clips (caf� noise, living room ambient, far-end music) with known ground truth for PESQ/STOI/SI-SDR baseline measurements.

9. **Device Compatibility**: "Mid-tier Snapdragon" reference hardware is defined as Snapdragon 700-series or better (e.g., SD 730, SD 765, SD 778) running Android 10+. Lower-end devices may not meet latency/CPU budgets.

10. **Offline Operation**: No network connectivity required during monitoring or recording. All processing (DSP, ML inference for denoiser/face detection) runs on-device. Analytics/telemetry upload is out of scope for this specification.

---

## Out of Scope

- **Call Signaling & VoIP Integration**: The app does not implement SIP, WebRTC, or telephony call control. It is a standalone DSP testbed, not a full communications client.

- **UI Theming & Branding**: UI is functional with basic Material Design components. Custom branding, dark mode, accessibility (beyond Android defaults), and advanced UI polish are deferred to future iterations.

- **Cloud Inference & Model Updates**: All ML models (denoiser, face detection) are bundled with the app. Over-the-air model updates, cloud-based inference, or federated learning are not included.

- **Multi-User AV-VAD**: AV-VAD is designed for single-user (primary speaker) scenarios. Multi-speaker face tracking and voice separation are out of scope.

- **Advanced Audio Formats**: Input/output is limited to PCM (linear). Compressed formats (Opus, AAC) and spatial audio (ambisonic, binaural) are not supported.

- **Persistent Session History**: Metrics and recordings are per-session. Long-term analytics dashboards, historical trend visualization, and cloud storage of recordings are out of scope.

- **Regulatory Compliance Testing**: Formal ITU-T/ETSI compliance testing (P.835, P.863, G.168) is not part of this feature specification. Informal metrics (PESQ, STOI) are used for internal validation only.

---

## Dependencies

- **Android SDK**: Minimum Android 10 (API level 29) for reliable low-latency audio APIs (AAudio) and scoped storage.

- **Audio Permissions**: `RECORD_AUDIO` permission is mandatory. Without it, the app cannot function.

- **Camera Permissions**: `CAMERA` permission is optional, required only for AV-VAD (A+). Core functionality works without it.

- **Hardware Requirements**: Device must support low-latency audio path (feature flag `android.hardware.audio.low_latency`). Most modern devices support this; very old or low-end devices may not.

- **NDK & Native Code**: DSP pipeline (AEC, denoiser, RES) and ML inference are implemented in C++ (via Android NDK) for performance. Java/Kotlin UI layer communicates with native layer via JNI.

- **On-Device ML Models**: Requires bundled models for denoiser (e.g., INT8 quantized RNN/CNN) and face/mouth detection (e.g., lightweight Haar cascades or MobileNet-based detector). Model size budget < 10 MB total.

- **Test Corpus**: Acceptance criteria assume availability of golden clip suite (audio test files) for objective metric validation. These clips must be prepared before acceptance testing begins.

---

## Risks & Mitigation

**Risk**: Device-specific audio HAL (Hardware Abstraction Layer) quirks may cause latency spikes or XRuns on certain devices.
**Mitigation**: Maintain a device compatibility matrix. Test on representative Snapdragon, Exynos, and MediaTek chipsets. Implement adaptive buffer sizing if needed.

**Risk**: AEC adaptive filter may diverge during double-talk, causing near-end speech suppression.
**Mitigation**: Robust DTD (double-talk detection) freezes or slows AEC adaptation during double-talk. Acceptance testing validates double-talk handling on golden clips.

**Risk**: AV-VAD face detection may fail in poor lighting, causing frequent fallback to audio-only VAD.
**Mitigation**: Graceful degradation is a core requirement (FR-027). Communicate AV-VAD unavailability in metrics overlay. Consider infrared or depth-based face detection for low-light scenarios in future iterations.

**Risk**: Recording 48 kHz WAV files for 30 seconds generates large files (~5 MB per file � 3 files = 15 MB per recording). Storage may fill up quickly.
**Mitigation**: Limit recording duration to 30 seconds max. Implement storage usage warning. Allow user to delete old recordings via in-app file manager.

**Risk**: CPU budget (< 6 ms per 10 ms hop) may be tight on lower-end devices, especially with AV-VAD camera processing.
**Mitigation**: Presets allow users to reduce CPU load (Battery saver mode). AV-VAD camera processing runs asynchronously on a separate thread; fusion only reads cached motion state. Profile extensively on mid-tier and low-end devices during implementation.

**Risk**: Objective metrics (PESQ, STOI) may not perfectly correlate with subjective user perception of audio quality.
**Mitigation**: Supplement objective metrics with informal listening tests (A/B comparison). Document known limitations of objective metrics. Consider adding subjective rating UI in future iterations.

---

## Notes

- This specification intentionally avoids implementation details (specific DSP algorithms, model architectures, Android API choices). Those decisions belong in the implementation plan phase.

- The three-preset system (Low-latency, Quality, Battery saver) provides flexibility without overwhelming users with per-parameter tuning. Actual parameter mappings (e.g., AEC filter length, denoiser aggressiveness) will be defined during implementation planning based on profiling data.

- A/B recording is critical for reproducibility and debugging. Three-file output (raw, far, enhanced) enables compute-once, analyze-many-times workflows for QA and researchers.

- AV-VAD (A+) is optional and marked as P3 priority. The app delivers full value without it. This allows phased development: ship core (P1+P2), then iterate on A+ if validated by early adopters.

- Metrics overlay is designed for technical users (developers, QA engineers), not end-users. Future consumer-facing apps may replace it with simplified quality indicators (e.g., 5-bar signal strength for audio quality).
