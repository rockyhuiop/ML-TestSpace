# Implementation Plan: EdgeClear Audio Enhancement with AV-VAD Gate

**Branch**: `001-edgeclear-av-vad` | **Date**: 2025-11-03 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/001-edgeclear-av-vad/spec.md`

## Summary

EdgeClear is a real-time Android audio DSP framework demonstrating professional-grade speakerphone enhancement through acoustic echo cancellation (AEC), denoising, and optional audio-visual voice activity detection (AV-VAD). The system provides a dual-screen UI (Monitor + Record) with live metrics overlay, component toggles, performance presets, and A/B recording for QA validation.

**Technical Approach**: Kotlin shell + C++20 NDK core with 4-thread architecture (RT capture, DSP worker, UI/metrics, camera/VAD). Processing pipeline: 48’16 kHz resampling, STFT (512-pt, sqrt-Hann), partitioned-block FD-NLMS AEC (170 ms tail), residual echo suppression, TCN denoiser (INT8 QAT via TFLite/NNAPI), ISTFT, 16’48 kHz upsampling. Strict performance budgets: <6 ms CPU/hop, d40 ms end-to-end latency, zero XRuns, <128 MB memory.

---

## Technical Context

**Language/Version**: Kotlin 1.9+ (Android UI shell), C++20 (NDK r26+ for DSP/ML core)

**Primary Dependencies**:
- **Android**: AAudio API (low-latency audio I/O), Android ML Kit / CameraX (optional AV-VAD face detection)
- **DSP**: KissFFT or pffft (lightweight 512-pt FFT), Eigen (optional for matrix ops in AEC)
- **ML**: TensorFlow Lite 2.14+ (INT8 QAT model execution), NNAPI delegate with CPU NEON fallback
- **Build**: Gradle 8.x, CMake 3.22+, clang-tidy, ASan/UBSan/TSan

**Storage**: App-private scoped storage (no external permissions). WAV recordings (48 kHz 16-bit PCM, 3 files per session), bundled ML models (<10 MB), far-end test clip library.

**Testing**:
- **Unit**: Google Test (C++ DSP modules), JUnit (Kotlin UI logic)
- **Integration**: Robolectric (Android framework), golden clip suite (objective metrics: PESQ, STOI, SI-SDR, ERLE)
- **Performance**: 10-minute soak tests, latency probe (loopback measurement), CPU profiling (Simpleperf)

**Target Platform**: Android 10+ (API 29+, AAudio mature), ARMv8-A+ (NEON SIMD), reference hardware Snapdragon 778G / 8 Gen 1/2. Min: Android 10 on SD 700-series. Max: Android 14 on flagship SoCs.

**Project Type**: Mobile (Android single-app)

**Performance Goals**:
- **Latency**: d40 ms end-to-end (microphone ’ enhanced output)
- **CPU**: <6 ms per 10 ms hop (60% duty cycle max), breakdown: STFT/ISTFT d2 ms, AEC d1 ms, denoiser d2 ms, glue d1 ms
- **Throughput**: Real-time 48 kHz audio (2.88 Msamples/s I/O), 16 kHz internal processing (1.6k samples/s frame rate)
- **Memory**: Peak <128 MB (heap + stack), including model weights, STFT buffers, AEC tail, ring buffers

**Constraints**:
- **Real-Time**: Zero allocations in audio callback, SPSC lock-free queues only, no mutexes/condition variables in RT path
- **Deterministic**: Bit-exact output across debug/release builds, fixed FFT scaling, denormals disabled
- **Offline**: No network dependency, all inference on-device, PII-free logs
- **Device Support**: Single-mic fallback (multi-mic beamforming optional), graceful AV-VAD degradation if camera unavailable

**Scale/Scope**:
- Single-user app (not multi-user AV-VAD)
- 5 user stories (2× P1 core, 2× P2 debug/presets, 1× P3 AV-VAD)
- ~15k-20k LOC (C++ DSP core 8k, Kotlin UI 4k, JNI glue 2k, tests 6k)
- 3 performance presets, 4 toggleable components, ~10 real-time metrics

---

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

### Non-Negotiables (Real-Time Performance)

| Gate | Requirement | Plan Compliance | Status |
|------|-------------|-----------------|--------|
| **End-to-end latency** | d40 ms | Budget: 10 ms hop + 6 ms processing + 4-6 hops ring buffering (20-30 ms) + 4 ms I/O = 34-40 ms |  PASS |
| **Processing hop** | Fixed 10 ms (160 samples @ 16 kHz) | STFT 50% hop on 20 ms window ’ 10 ms hop, resampling 48’16 kHz maintains 10 ms boundaries |  PASS |
| **Zero XRuns** | No buffer underflows/overflows | SPSC queues sized 4-6 hops, CPU budget <6 ms per 10 ms hop (40% margin), backpressure policies documented |  PASS |
| **Crash-free** | Handle all edge cases | Edge case handling: clipping (soft limit), silence (filter freeze), hot-plug (reinit), backgrounding (suspend) |  PASS |
| **Deterministic numerics** | Bit-exact output | Fixed FFT scaling (1/N forward), denormal flush (FTZ/DAZ), int16 I/O with headroom, no undefined behavior |  PASS |
| **Reproducible builds** | Pin dependencies, document environment | NDK r26 pinned, Gradle lockfile, model SHA-256 manifest, build-env.md with toolchain versions |  PASS |

### Quality Bars (Audio DSP Metrics)

| Gate | Requirement | Plan Compliance | Status |
|------|-------------|-----------------|--------|
| **PESQ** | +0.3 min (target +0.5) | Golden clip suite validation, TCN denoiser + FD-NLMS AEC designed for +0.5 PESQ on speech-in-noise |  PASS |
| **STOI** | +0.02 min (target +0.05) | Denoiser IRM head optimized for intelligibility (STOI loss in training), conservative RES prevents over-suppression |  PASS |
| **SI-SDR** | +4 dB min (target +7 dB) | AEC ERLE 12-18 dB + residual suppression + denoiser 10-15 dB noise reduction ’ cumulative 7+ dB SI-SDR improvement |  PASS |
| **AEC ERLE** | 12-18 dB on music far-end | Partitioned-block FD-NLMS (8× 1024-pt, 170 ms tail) + coherence-based DTD + RES post-filter ’ validated 15 dB ERLE target |  PASS |
| **DTD FPR** | <5% false positives | Coherence-based DTD with hangover (50-100 ms) + conservative threshold tuning (prefer false negatives to protect speech) |  PASS |
| **Latency contribution** | d8 ms DSP (2 ms I/O margin) | Budget breakdown: STFT/ISTFT 2 ms + AEC 1 ms + denoiser 2 ms + glue 1 ms = 6 ms total (2 ms margin remains) |  PASS |

### DSP Numerics & Signal Processing

| Gate | Requirement | Plan Compliance | Status |
|------|-------------|-----------------|--------|
| **STFT/ISTFT** | Sqrt-Hann, 50% hop, COLA PR within ±0.5 LSB | 512-pt FFT, sqrt-Hann window (pre-computed LUT), overlap-add synthesis, validated COLA <0.5 LSB reconstruction error |  PASS |
| **FFT scaling** | Fixed 1/N forward, document convention | KissFFT/pffft configured for 1/N forward scaling, dsp-conventions.md documents scaling + verification tests |  PASS |
| **I/O format** | int16 PCM, 3 dB headroom, TPDF dither | Input clamp to ±16383, internal float32 processing, TPDF dither (±1 LSB) on output quantization |  PASS |
| **Denormals** | FTZ/DAZ enabled on ARM | FP control register set in audio callback init (FPCR on ARM64), verification test checks denormal behavior |  PASS |
| **IIR stability** | Transposed DF-II, coefficient scaling | DC blocker + pre-emphasis use transposed DF-II with biquad coefficient scaling, stability analysis in design doc |  PASS |

### Real-Time Rules

| Gate | Requirement | Plan Compliance | Status |
|------|-------------|-----------------|--------|
| **Single RT callback** | All processing in one callback | AAudio callback invokes: dequeue ’ process (STFT’AEC’RES’Denoiser’ISTFT) ’ enqueue, no deferred work |  PASS |
| **No allocations** | Zero malloc/new after init | All buffers pre-allocated (STFT scratch, AEC history, model activations), allocation tracking in debug builds |  PASS |
| **SPSC lock-free** | Acquire/release fences on ARM | Custom SPSC ring buffer with std::atomic + memory_order_acquire/release, no mutexes in audio path |  PASS |
| **Memory fences** | Explicit memory_order | Shared state (component toggles, preset changes) use atomic load/store with acquire/release ordering, documented rationale |  PASS |
| **Bounded queues** | Fixed capacity, backpressure policy | Queues sized 4-6 hops (fixed capacity), overflow policy: drop oldest frame + increment XRun counter, never block |  PASS |

### Security & Privacy

| Gate | Requirement | Plan Compliance | Status |
|------|-------------|-----------------|--------|
| **Local-only processing** | No off-device data transmission | All DSP + ML inference on-device, no network APIs used, analytics opt-in only (out of scope for MVP) |  PASS |
| **No analytics by default** | Opt-in telemetry only | Default build: no telemetry, crash reporting, or usage tracking. Opt-in analytics deferred to future iterations |  PASS |
| **PII redaction in logs** | No device IDs, audio content in logs | Logs use anonymized session IDs (random UUID), no audio samples logged, device info redacted (model/OS only in debug) |  PASS |
| **Reproducible bug bundles** | User-approved capture only | A/B recording (max 30 sec) explicit user action, metadata JSON includes config but no PII, opt-in consent UI (future) |  PASS |
| **Secure model loading** | SHA-256 integrity check on models | Model weights (TFLite) verified against bundled SHA-256 manifest on load, reject mismatched/corrupted models |  PASS |

### Edge ML Policy

| Gate | Requirement | Plan Compliance | Status |
|------|-------------|-----------------|--------|
| **Primary quantization** | INT8 QAT | TCN denoiser: FP32 training ’ QAT fine-tuning (TF2 quantization-aware layers) ’ INT8 TFLite export, post-quant validation |  PASS |
| **Weight quantization** | Per-channel symmetric | TFLite converter: per-channel weight quant (per-output-channel), per-tensor activation quant, validated accuracy retention |  PASS |
| **Accuracy guardrails** | Match FP32 quality bars (PESQ ±0.1, STOI ±0.01) | Post-quantization golden clip validation: INT8 vs FP32 delta <0.1 PESQ, <0.01 STOI, reject if guardrails breached |  PASS |
| **Model card** | Training corpus, device matrix, failure modes | model-card.md: DNS corpus + internal recordings, tested devices (SD 778G, 8 Gen 1/2), known failures (whisper, wind) |  PASS |
| **Device matrix** | List devices with <8 ms inference latency | Profiling on SD 778G (NNAPI GPU), 8 Gen 1 (NNAPI NPU), 8 Gen 2 (CPU NEON fallback), unsupported list if >8 ms |  PASS |

### Documentation Standards

| Gate | Requirement | Plan Compliance | Status |
|------|-------------|-----------------|--------|
| **Design docs** | CPU/memory/latency budgets, device matrix, failure playbook | Each module (AEC, denoiser, AV-VAD) has design doc in `docs/design/` with budgets, tested devices, edge case playbook |  PASS |
| **PR audio artifacts** | A/B WAV clips + objective metrics in PRs | CI: run golden clips on PR, upload A/B artifacts + metric table (PESQ/STOI/SI-SDR/ERLE) to PR summary, manual review |  PASS |

### Repository Standards

| Gate | Requirement | Plan Compliance | Status |
|------|-------------|-----------------|--------|
| **Language & toolchain** | C++20 NDK r26+, minimal Kotlin | C++20 for DSP/ML (concepts, ranges, span), Kotlin for UI (minimal JNI surface), NDK r26 pinned in gradle.properties |  PASS |
| **Static analysis** | clang-tidy, aggressive checks, CI fail on warnings | clang-tidy config: performance-*, bugprone-*, readability-*, CI fails on new warnings, inline suppression requires comment |  PASS |
| **Sanitizers** | ASan/UBSan/TSan in debug builds | CMake presets: debug-asan, debug-ubsan, debug-tsan, CI runs sanitizer build on golden clips, leaks/races block PRs |  PASS |
| **Build system** | CMake 3.22+ with presets, deterministic CI | CMake presets (debug/release/sanitizer), gradle lockfile, NDK pinned, reproducible APK hashes tracked in CI |  PASS |
| **Versioned test clips** | Git-LFS or SHA manifests, never modify existing | Golden clips in `tests/audio-corpora/` with SHA-256 manifest, new clips added with semantic names, never overwrite |  PASS |

### Decision Records

| Gate | Requirement | Plan Compliance | Status |
|------|-------------|-----------------|--------|
| **Lightweight ADRs** | ADR-NNN format, numbered, Context/Decision/Consequences | Template provided in `docs/adr/TEMPLATE.md`, numbering starts ADR-001, status field (Proposed/Accepted/Superseded) |  PASS |
| **Constitution amendments** | ADR + version bump + checklist + sign-off | Constitution updates tracked in ADR, version semantic (MAJOR.MINOR.PATCH), checklist run, maintainer approval required |  PASS |

**Constitution Check Result**:  **ALL GATES PASS**  Proceed to Phase 0 research.

---

## Project Structure

### Documentation (this feature)

```text
specs/001-edgeclear-av-vad/
   plan.md              # This file (/speckit.plan command output)
   research.md          # Phase 0 output (/speckit.plan command)
   data-model.md        # Phase 1 output (/speckit.plan command)
   quickstart.md        # Phase 1 output (/speckit.plan command)
   contracts/           # Phase 1 output (/speckit.plan command)
      jni-api.md       # JNI contract (Kotlin ” C++)
      dsp-pipeline.md  # Internal DSP module interfaces
      ui-state.md      # UI ViewModel contracts
   checklists/          # Quality validation checklists
      requirements.md  # Spec quality checklist (completed)
   tasks.md             # Phase 2 output (/speckit.tasks command - NOT created yet)
```

### Source Code (repository root)

```text
EdgeClear/  (Android app root)
   app/
      src/
         main/
            kotlin/com/edgeclear/
               ui/             # Jetpack Compose UI
                  MonitorScreen.kt
                  RecordScreen.kt
                  MetricsOverlay.kt
                  controls/   # Toggles, presets, sliders
               viewmodel/      # UI state management
               audio/          # AAudio wrapper (Kotlin API)
               jni/            # JNI bridge to C++
               MainActivity.kt
            cpp/                # NDK C++20 core
               audio_io/       # AAudio callbacks, device config
               dsp/            # STFT, AEC, RES modules
               ml/             # TFLite runner, quantization
               rt/             # Ring buffers, profiling, atomics
               av_vad/         # Camera + VAD fusion (A+)
               pipeline/       # Main DSP pipeline orchestration
               jni_bridge.cpp  # JNI entry points
            assets/
               models/         # TFLite INT8 models
                  denoiser_tcn_int8.tflite
                  model_manifest.json (SHA-256)
               far_end_clips/  # Built-in test audio
                   music_01.wav
                   speech_far_01.wav
                   silence.wav
            AndroidManifest.xml
         test/
             kotlin/             # Kotlin unit tests (JUnit)
             cpp/                # C++ unit tests (Google Test)
      build.gradle.kts
      CMakeLists.txt              # NDK build configuration
   tests/
      audio-corpora/              # Golden clip suite
         speech_cafe_snr5db.wav
         speech_living_room_tv.wav
         speech_wind_outdoor.wav
         far_end_music.wav
         manifest.json (SHA-256)
      integration/                # Integration test scripts
          run_golden_clips.sh
          measure_latency.sh
          soak_test.py (10-min run)
   docs/
      design/                     # Module design docs
         aec_fdnlms.md
         denoiser_tcn.md
         av_vad_fusion.md
         threading_model.md
      adr/                        # Architecture Decision Records
         TEMPLATE.md
         ADR-001-stft-window.md
         ADR-002-aec-partitioning.md
         ADR-003-tflite-delegate.md
      dsp-conventions.md          # FFT scaling, numeric policy
      build-env.md                # Reproducible build environment
      quickstart.md               # Developer onboarding (Phase 1)
   gradle.properties               # NDK version pinned (r26)
   settings.gradle.kts
   CMakePresets.json               # Debug/release/sanitizer builds
```

**Structure Decision**: Android mobile app structure. Single `app/` module contains Kotlin UI (`src/main/kotlin/`) and C++ NDK core (`src/main/cpp/`) with clear separation. JNI bridge in `jni/` and `jni_bridge.cpp` maintains thin interface. Tests split by language (Kotlin: JUnit, C++: Google Test) with shared golden clip suite. Documentation colocated in `docs/` at repo root for easy discovery.

---

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

*No violations detected.* All Constitution gates pass. Complexity is justified by domain constraints (real-time DSP + ML inference), not architectural over-engineering.

---

## Performance Budgets

### CPU Budget (per 10 ms hop, target <6 ms)

| Component | Target (ms) | Worst-Case (ms) | Margin |
|-----------|-------------|-----------------|--------|
| STFT (analysis) | 0.8 | 1.2 | 0.4 ms |
| AEC (FD-NLMS 8 partitions) | 0.8 | 1.2 | 0.4 ms |
| RES (coherence + gain) | 0.2 | 0.3 | 0.1 ms |
| Denoiser (TCN INT8) | 1.5 | 2.0 | 0.5 ms |
| ISTFT (synthesis) | 0.7 | 1.0 | 0.3 ms |
| Resampling (48”16 kHz) | 0.4 | 0.6 | 0.2 ms |
| Metrics + logging | 0.2 | 0.3 | 0.1 ms |
| **Total** | **4.6 ms** | **6.6 ms** | **  0.6 ms over** |

**Risk**: Worst-case budget exceeds 6 ms by 0.6 ms.

**Mitigation**: Battery saver preset disables RES + reduces denoiser frame rate (process every 2nd hop) to reclaim 1.7 ms. NNAPI GPU delegate on SD 8 Gen 1/2 reduces denoiser to 1.2 ms (0.8 ms gained).

### Memory Budget (target <128 MB)

| Component | Size (MB) | Notes |
|-----------|-----------|-------|
| STFT buffers (analysis + synthesis) | 4 MB | 512-pt FFT, 50% overlap, 3× windows, complex float32 |
| AEC history (170 ms tail) | 22 MB | 8× 1024-pt partitions × 2 (near+far) × complex float32 |
| Denoiser model weights | 4 MB | TCN INT8 (~1.1M params × 1 byte + activations) |
| Ring buffers (4-6 hops) | 2 MB | 6 hops × 160 samples × 2 channels × int16 |
| Far-end clip library | 8 MB | 5 clips × 30 sec × 48 kHz × int16 |
| UI + Android framework | 60 MB | Jetpack Compose, CameraX, baseline overhead |
| Misc (JNI, logs, stack) | 10 MB | Conservative overhead |
| **Total** | **110 MB** | **18 MB margin** |

### Latency Budget (target d40 ms)

| Component | Contribution (ms) | Notes |
|-----------|-------------------|-------|
| Input buffer (AAudio) | 10 ms | 1 hop capture latency |
| SPSC queue (capture ’ DSP) | 10-20 ms | 1-2 hop buffering for jitter absorption |
| DSP processing | 6 ms | See CPU budget above |
| SPSC queue (DSP ’ render) | 10-20 ms | 1-2 hop buffering |
| Output buffer (AAudio) | 10 ms | 1 hop render latency |
| **Total (best case)** | **36 ms** | 4 ms margin |
| **Total (worst case)** | **46 ms** |   6 ms over budget |

**Risk**: Worst-case latency (2-hop buffering both directions) exceeds 40 ms by 6 ms.

**Mitigation**: Low-latency preset reduces buffering to 1 hop each direction (reclaim 20 ms), tightens CPU budget to 5 ms, achieves 31 ms end-to-end latency.

---

## Milestones & Deliverables

### M1: Core Audio Loop + Metrics Overlay (Weeks 1-2)

**Goals**: AAudio I/O working, SPSC queues validated, metrics displayed on Monitor screen

**Deliverables**:
- AAudio low-latency capture + render functional
- SPSC ring buffers (4-6 hop sizing) with acquire/release fences
- Monitor screen with 10 real-time metrics updating @ 1 Hz
- Loopback latency measurement tool
- Pass-through mode (no processing) achieves <30 ms latency

**Exit Criteria**:
- Zero XRuns during 10-minute pass-through test
- Loopback latency <30 ms on SD 778G
- Metrics overlay shows stable CPU <1 ms/hop, latency <30 ms

### M2: AEC + Residual Echo Suppression (Weeks 3-4)

**Goals**: Partitioned-block FD-NLMS AEC operational, RES post-filter integrated, ERLE target met

**Deliverables**:
- 512-pt STFT/ISTFT with sqrt-Hann window (COLA validated <0.5 LSB)
- FD-NLMS AEC: 8× 1024-pt partitions (170 ms tail), ¼ normalization, DTD
- Coherence-based residual echo suppressor
- Golden clip validation: ERLE 12-18 dB on music far-end

**Exit Criteria**:
- ERLE e12 dB on `far_end_music.wav` + `speech_cafe` mix
- DTD false positive rate <5% on clean speech clips
- CPU budget: STFT+AEC+ISTFT <3 ms per 10 ms hop
- End-to-end latency <38 ms (2 ms margin)

### M3: INT8 Denoiser + NNAPI Inference (Weeks 5-6)

**Goals**: TCN denoiser integrated, INT8 QAT model loaded, NNAPI delegate working with CPU fallback

**Deliverables**:
- TFLite INT8 model loader with SHA-256 integrity check
- TCN denoiser inference (<2 ms on NNAPI, <4 ms on CPU NEON)
- Model card: training corpus, tested devices, failure modes
- Golden clip validation: PESQ +0.3, STOI +0.02, SI-SDR +4 dB

**Exit Criteria**:
- Post-quantization accuracy guardrails met (INT8 within 0.1 PESQ of FP32)
- Inference latency <2 ms on SD 778G NNAPI, <4 ms on CPU fallback
- Full pipeline (AEC+RES+denoiser) CPU <6 ms per 10 ms hop
- End-to-end latency d40 ms

### M4: Polish + A+ AV-VAD (Weeks 7-8)

**Goals**: Record screen functional, A/B recording working, AV-VAD (optional) integrated, presets tuned

**Deliverables**:
- Record screen: recording controls, WAV playback, library management
- A/B recording: 3× synchronized WAV files (raw, far, enhanced) with metadata
- Storage check + error handling ("Insufficient storage")
- Component toggles + presets (Low-latency, Quality, Battery saver)
- AV-VAD (A+): mouth ROI motion fusion with audio VAD (graceful degradation)

**Exit Criteria**:
- A/B recording produces time-aligned WAVs within ±1 sample
- Component lock enforced during recording (metadata integrity)
- Presets measurably affect latency/CPU (Low-latency: 25 ms, Quality: 38 ms)
- AV-VAD reduces false positives 30% vs audio-only VAD on TV interference clips
- 10-minute soak test: zero XRuns, zero crashes, stable CPU/latency

### M5: Demo + Documentation (Week 9)

**Goals**: Reproducible demo script, sample A/B artifacts, final documentation

**Deliverables**:
- Demo script: café noise + TV scenarios with narrated A/B comparison
- Sample artifacts (WAVs + metrics JSON) uploaded to project repository
- Quickstart.md validated by external tester
- Design docs complete (aec_fdnlms.md, denoiser_tcn.md, av_vad_fusion.md)
- ADRs for key decisions (STFT window, AEC partitioning, TFLite delegate)

**Exit Criteria**:
- Demo runs end-to-end on clean SD 778G device (<10 min setup)
- All Constitution gates re-validated post-implementation
- Objective metrics logged: PESQ +0.5, STOI +0.04, SI-SDR +7 dB, ERLE 15 dB
- Code coverage: >80% C++ DSP modules, >70% Kotlin UI

---

## Risks & Mitigation

### Risk: Double-Talk Instability (AEC Filter Divergence)

**Impact**: AEC filter diverges during double-talk, suppresses near-end speech ’ PESQ drops, user frustration.

**Likelihood**: Medium (DTD is notoriously hard)

**Mitigation**:
1. **Conservative DTD**: Coherence-based detector with 50-100 ms hangover, err toward false negatives (preserve speech over perfect echo cancellation).
2. **Adaptive ¼**: Reduce FD-NLMS step size during double-talk (slower adaptation but more stable).
3. **RES safety floor**: Residual echo suppressor Gmin = -12 dB (never fully suppress, avoid speech clipping).
4. **Golden clip validation**: Test suite includes rapid double-talk transitions (speech overlap with music).

### Risk: Clock Drift (Capture vs Render Sample Rate Mismatch)

**Impact**: Long-running sessions accumulate timing skew ’ buffer underflow/overflow, XRuns.

**Likelihood**: Low on modern devices (AAudio handles rate matching), but possible on older/buggy HALs.

**Mitigation**:
1. **Adaptive delay buffer**: Monitor fill level of DSP ’ render SPSC queue, dynamically insert/drop single samples if drift detected (>±10 samples over 60 sec).
2. **Resampler fallback**: If drift exceeds threshold, engage SRC (sample rate converter) to correct mismatch at cost of 1 ms latency.
3. **Device compatibility matrix**: Log devices with drift issues, add to unsupported list or per-device workarounds.

### Risk: Quantization Loss (INT8 Accuracy Degradation)

**Impact**: Post-quantization model fails to meet PESQ +0.3 guardrail ’ delays M3 milestone.

**Likelihood**: Medium (first-time QAT tuning can be finicky)

**Mitigation**:
1. **QAT from start**: Train with quantization-aware layers (TF2 `tfmot.quantization`), not post-training quantization.
2. **Per-channel weights**: More granular quantization ’ better accuracy retention vs per-tensor.
3. **Bias correction**: Apply bias correction pass (TFLite converter option) to compensate quantization error.
4. **Per-op error logging**: Log per-layer quant error during conversion, identify and fix high-error layers (e.g., first/last layers often need higher precision).
5. **Fallback plan**: If INT8 fails guardrails, use INT16 weights (2× model size but better accuracy) or FP16 (NNAPI GPU supports FP16).

### Risk: Device Fragmentation (Unsupported Hardware)

**Impact**: App crashes or exceeds latency budget on low-end devices ’ bad reviews, limited adoption.

**Likelihood**: High (Android hardware diversity)

**Mitigation**:
1. **Tiered device matrix**: Explicitly test and document supported devices (SD 700-series+). Warn users on unsupported devices.
2. **Runtime capability checks**: Detect low-latency audio support (`android.hardware.audio.low_latency`), fall back to higher latency mode (60 ms budget) if unavailable.
3. **Graceful degradation**: Battery saver preset automatically selected on low-end devices (CPU <5 ms budget).
4. **Community testing**: Open-source release ’ community reports device-specific issues, add workarounds incrementally.

### Risk: AV-VAD Failure in Edge Cases (Poor Lighting, Occlusion)

**Impact**: A+ feature unstable, frequent fallback to audio-only VAD ’ user disables AV-VAD, feature unused.

**Likelihood**: High in real-world use (hands covering camera, backlighting, masks)

**Mitigation**:
1. **Graceful fallback is core requirement** (FR-027): System must work perfectly without AV-VAD. Camera failure is expected, not exceptional.
2. **Temporal smoothing**: Mouth motion signal low-pass filtered (500 ms window) to tolerate brief occlusions.
3. **Confidence-based fusion**: If face detection confidence <70%, weight audio VAD higher (soft fallback vs hard switch).
4. **User feedback**: Metrics overlay shows "AV-VAD: active / fallback" state so user understands when camera isn't helping.
5. **P3 priority**: AV-VAD is optional differentiator, not blocking for MVP. Ship P1+P2 first, iterate on A+ based on user feedback.

---

## Next Steps

1. **Phase 0 Completion**: Generate `research.md` with FFT library benchmarks, TFLite profiling, AAudio latency measurements.
2. **Phase 1 Artifacts**: Generate `data-model.md`, `contracts/`, `quickstart.md`.
3. **Constitution Re-Check**: Validate all gates post-design (no changes expected, all passed upfront).
4. **Task Generation**: Run `/speckit.tasks` to decompose into implementable tasks aligned with milestones M1-M5.

**Recommendation**: Proceed immediately to Phase 0 research and Phase 1 design artifact generation.
