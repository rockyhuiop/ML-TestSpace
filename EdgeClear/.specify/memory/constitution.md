<!--
Sync Impact Report
==================
Version Change: N/A → 1.0.0 (Initial Constitution)
Rationale: First comprehensive constitution for EdgeClear/EdgeClear-AV project

Modified Principles: N/A (initial creation)
Added Sections:
  - Purpose & Scope
  - Non-Negotiables (Real-Time Performance)
  - Quality Bars (Audio DSP Metrics)
  - DSP Numerics & Signal Processing
  - Real-Time Rules
  - Security & Privacy
  - Edge ML Policy
  - Documentation Standards
  - Repository Standards
  - Decision Records

Removed Sections: N/A

Templates Requiring Updates:
  ✅ plan-template.md - Already contains Constitution Check section
  ✅ spec-template.md - No updates required (template-agnostic, principles apply)
  ✅ tasks-template.md - No updates required (task separation already compatible)

Follow-up TODOs: None

Amendment Tracking:
  - This is the initial ratification
  - Future amendments must follow the Amendment Procedure below
  - All amendments require re-running Constitution Update Checklist
-->

# EdgeClear / EdgeClear-AV Constitution

## Purpose & Scope

**EdgeClear** is a showcase-quality real-time communications DSP framework for Android devices (phones and tablets). The project demonstrates professional-grade speakerphone enhancement combining acoustic echo cancellation (AEC), denoising, and optional lip-aware VAD gating—entirely on-device without cloud dependencies.

**Scope**: All audio processing must execute locally on the device. No microphone data, intermediate features, or inference results may be transmitted off-device during normal operation. The framework targets real-time duplex communications scenarios where end-to-end latency and audio quality directly impact user experience.

**Target Platform**: Android 8.0+ (API level 26+) on phones and tablets with ARMv8-A+ CPUs. All inference runs on-device using optimized INT8 models.

---

## Core Principles

### I. Non-Negotiables (Real-Time Performance)

**End-to-end latency**: MUST remain ≤ 40 ms under all supported configurations (AEC + denoiser + VAD) on reference hardware.

**Processing hop**: Fixed 10 ms frame size (160 samples @ 16 kHz). Non-negotiable for predictable scheduling and minimal buffering.

**Zero XRuns**: The real-time audio callback MUST complete processing within the hop duration. XRuns (buffer underflows/overflows) are unacceptable in production builds; any detected XRun MUST be logged, traced, and treated as a critical defect.

**Crash-free**: Audio processing pipeline MUST handle all edge cases (silence, clipping, discontinuities, device hot-plug) without crashing or entering undefined states.

**Deterministic numerics**: Given identical input buffers and initial state, the DSP pipeline MUST produce bit-exact output across debug/release builds and supported devices. Use fixed-point FFT scaling; disable denormal flushing explicitly.

**Reproducible builds**: All binary artifacts (native libraries, APKs) MUST be reproducible from tagged source commits. Pin all dependencies (NDK version, CMake, Gradle, model weights); document build environment in `docs/build-env.md`.

**Rationale**: Real-time audio processing is unforgiving. Missing any of these requirements results in user-perceptible glitches, degraded call quality, or app crashes during calls—all of which destroy trust in the product.

---

### II. Quality Bars (Audio DSP Metrics)

All metrics measured on standardized test sets (defined in `tests/audio-corpora/`) against noisy baseline (no processing).

**Perceptual quality (PESQ)**: +0.3 minimum improvement over noisy baseline. Target: +0.5 on speech-in-noise scenarios.

**Intelligibility (STOI)**: +0.02 minimum improvement. Target: +0.05 on low-SNR (0–5 dB) conditions.

**Signal fidelity (SI-SDR)**: +4 dB minimum improvement. Target: +7 dB on stationary noise.

**AEC performance (ERLE)**: 12–18 dB echo return loss enhancement on music far-end playback (standardized music clips in `tests/audio-corpora/music/`). MUST handle rapid double-talk transitions without clipping near-end speech.

**Double-talk detection (DTD)**: False positive rate < 5% on clean speech. False negatives acceptable if they preserve near-end speech (conservative bias).

**Latency contribution**: AEC adaptive filter + denoiser inference + VAD must fit within 8 ms on reference hardware (Snapdragon 8 Gen 1), leaving 2 ms margin for I/O and scheduling jitter.

**Rationale**: These thresholds represent the minimum improvement users can reliably perceive. Lower improvements waste compute; excessive processing risks artifacts or latency violations. Music far-end is the hardest AEC challenge; passing it ensures robustness.

---

### III. DSP Numerics & Signal Processing

**STFT/ISTFT**: Sqrt-Hann window (sqrt of Hann) with 50% hop overlap. MUST satisfy COLA (Constant Overlap-Add) perfect reconstruction within ±0.5 LSB (int16).

**FFT scaling**: Use fixed forward FFT scaling (1/N normalization on forward transform, identity on inverse). Document scaling convention in `docs/dsp-conventions.md`.

**I/O format**: int16 PCM input/output. Reserve 3 dB headroom (±16383 max magnitude) to prevent wrap-around on summation. Apply dither (TPDF, ±1 LSB) on down-conversion from internal float32/int32 to output int16.

**Denormals**: Explicitly disable denormal floats on ARM (set FTZ and DAZ flags in FP control register) to prevent CPU stalls. Verify in audio callback initialization.

**Numerical stability**: All IIR filters (pre-emphasis, DC removal) MUST use transposed Direct Form II with coefficient scaling to avoid overflow. Document stability analysis in design docs.

**Rationale**: COLA ensures reconstruction; fixed FFT scaling ensures determinism; int16 headroom prevents clipping; denormal control prevents performance collapse; transposed DF-II prevents filter instability. These are standard but non-obvious practices that prevent subtle bugs.

---

### IV. Real-Time Rules

**Single audio callback**: All processing (AEC, denoiser, VAD) MUST complete within one invocation of the real-time audio callback. No deferred processing, background threads, or split-phase work.

**No allocations after init**: Zero heap allocations (malloc/new) in the audio callback or any code path reachable from it. Pre-allocate all buffers, queues, and model scratch space during initialization. Verify with allocation tracing in debug builds.

**SPSC lock-free queues**: Inter-thread communication (audio thread ↔ control thread) MUST use Single-Producer Single-Consumer lock-free queues with explicit memory ordering (acquire/release semantics on ARM). No mutexes, condition variables, or spin-locks in audio path.

**Memory fences**: All shared state updates MUST use std::atomic with explicit memory_order (prefer acquire/release over seq_cst for performance). Document fence placement rationale in code comments.

**Bounded queues with backpressure**: All queues MUST have fixed capacity. On overflow, drop oldest samples or apply a documented backpressure policy (e.g., signal control thread to reduce load). Never block or allocate.

**Rationale**: Real-time audio cannot tolerate unpredictable delays. Allocations trigger GC or page faults; locks cause priority inversion; unbounded queues cause memory growth. These rules are standard in pro-audio but require discipline.

---

### V. Security & Privacy

**Local-only processing**: Microphone data, spectral features, and model activations MUST NOT leave the device during normal operation. Exception: opt-in bug reporting (see below).

**No analytics by default**: Usage telemetry, crash reporting, and audio logging are opt-in only. Default build collects nothing. Opt-in consent must be explicit, informed, and revocable.

**PII redaction in logs**: Log messages MUST NOT contain device identifiers, user names, phone numbers, or audio content. Use hashed IDs or anonymized tokens if correlation is needed.

**Reproducible bug bundles**: When user opts in to bug reporting, capture: logs, device metadata (model, OS, CPU), configuration state, and optionally a short audio snippet (max 5 seconds, user-approved). No unredacted crashes without consent.

**Secure model loading**: Model weights stored in APK assets MUST be integrity-checked (SHA-256 hash) before loading. Reject tampered or mismatched models. Log verification failures.

**Rationale**: Microphone data is highly sensitive. Local-only processing is a key differentiator and trust signal. PII leaks violate user privacy and legal requirements (GDPR, CCPA). Reproducible bug bundles balance debuggability with privacy.

---

### VI. Edge ML Policy

**Primary quantization**: INT8 Quantization-Aware Training (QAT) for the denoiser model. FP32 training → QAT fine-tuning → INT8 export. Validate accuracy post-quantization on held-out test set.

**Weight quantization**: Per-channel (per-output-channel) symmetric weight quantization. Per-tensor quantization acceptable for activations if accuracy targets still met.

**Accuracy guardrails**: Post-quantization model MUST meet the same Quality Bars (Section II) as FP32 baseline. If INT8 model degrades PESQ > 0.1 or STOI > 0.01 vs. FP32, reject and retune quantization.

**Model card**: Every deployed model MUST have a model card documenting: training corpus, hyperparameters, quantization scheme, tested device matrix (CPU models, Android versions), known failure modes (e.g., specific noise types, far-end echo profiles), and accuracy metrics on standard test sets.

**Device matrix**: Model card MUST list devices where inference latency < 8 ms. If new device fails latency target, add to "unsupported devices" list and gate in runtime device check.

**Rationale**: INT8 QAT balances accuracy and performance. Per-channel weight quantization is a sweet spot for small models. Accuracy guardrails prevent regressions. Model cards ensure traceability and set user expectations. Device matrix prevents deploying on hardware that can't meet latency SLA.

---

### VII. Documentation Standards

**Design documents**: Every non-trivial feature (new DSP block, model architecture, queue policy) MUST have a design doc in `docs/design/` with:

- **CPU budget**: ms per 10 ms hop, measured on reference device (Snapdragon 8 Gen 1).
- **Memory peak**: Heap + stack usage, including scratch buffers.
- **Latency budget**: Contribution to end-to-end latency (algorithmic + lookahead).
- **Device matrix**: Tested and supported devices (specific models, not just chipset families).
- **Failure playbook**: Known edge cases and mitigation strategies (e.g., "VAD fails on whispered speech → expected, document as limitation").

**PR audio artifacts**: Pull requests modifying DSP or ML components MUST include:

- **A/B clips**: Before/after WAV files (uploaded to PR or linked artifact storage). Clips should demonstrate the change on representative test cases.
- **Objective metrics**: PESQ, STOI, SI-SDR, ERLE on standard test set (or subset if full sweep is too expensive). Include in PR description or CI summary.

**Rationale**: Design docs force upfront thinking about performance and failure modes. CPU/memory/latency budgets are non-negotiable constraints; documenting them prevents integration surprises. Audio A/B artifacts make reviews tractable (listening > reading spectrogram descriptions). Objective metrics catch regressions.

---

### VIII. Repository Standards

**Language & toolchain**: C++20 for DSP/ML (NDK r26+), minimal Java/Kotlin for Android glue. C++20 features (concepts, ranges, coroutines) allowed if they simplify code without runtime cost.

**Static analysis**: clang-tidy with aggressive checks (performance-\*, bugprone-\*, readability-\*). CI MUST fail on new warnings. Suppression requires inline comment justification.

**Sanitizers**: ASan, UBSan, TSan enabled in debug builds. CI runs sanitizers on standard test suite. Memory leaks, data races, and undefined behavior block PRs.

**Build system**: CMake 3.22+ with presets for debug/release/sanitizer builds. All configuration in CMakeLists.txt or presets JSON—no undocumented environment variables. Pin NDK version in gradle.properties.

**Deterministic CI**: Build artifacts (native libs, APKs) MUST be reproducible across CI runs. Pin all dependencies (Gradle, NDK, model weights from fixed URLs/SHAs). Fail CI if hash mismatch.

**Versioned test clips**: Audio test corpora in `tests/audio-corpora/` MUST be versioned (git-lfs or external storage with SHA manifests). Never modify existing clips; add new ones with semantic names (e.g., `speech_cafe_snr5db_v2.wav`).

**Rationale**: C++20 modernizes code safely; clang-tidy catches bugs; sanitizers catch memory/threading errors; CMake presets simplify onboarding; deterministic CI ensures reproducibility; versioned test clips prevent "works on my machine" due to corpus drift.

---

### IX. Decision Records

**Lightweight ADRs**: Significant technical decisions (DSP algorithm selection, model architecture, queue policy) MUST be captured in Architecture Decision Records in `docs/adr/`. Format:

```
# ADR-NNN: [Title]
Date: YYYY-MM-DD
Status: [Proposed | Accepted | Deprecated | Superseded by ADR-MMM]

## Context
[What problem are we solving? Why now?]

## Decision
[What did we decide? Be specific.]

## Consequences
[Trade-offs. What did we gain? What did we lose?]
```

**Numbering**: Sequential (ADR-001, ADR-002, …). Never reuse numbers. Deprecated ADRs stay in repo with updated Status.

**Constitution amendments**: Changes to this Constitution MUST:

1. Create an ADR documenting the rationale.
2. Increment CONSTITUTION_VERSION (major/minor/patch per semantic rules below).
3. Run the "Constitution Update Checklist" (see Amendment Procedure).
4. Get sign-off from project maintainers (in PR review or decision log).

**Rationale**: ADRs provide context for future maintainers. Lightweight format lowers friction. Constitution amendments are governance changes requiring explicit approval.

---

## Governance

### Amendment Procedure

This Constitution is version-controlled at `.specify/memory/constitution.md`. Amendments follow this procedure:

1. **Propose**: Open a PR modifying `constitution.md`. Include ADR documenting rationale.
2. **Increment version**: Update `CONSTITUTION_VERSION` following semantic versioning:
   - **MAJOR** (X.0.0): Backward-incompatible changes (removing principles, redefining non-negotiables).
   - **MINOR** (x.Y.0): Additive changes (new principles, expanded guidance).
   - **PATCH** (x.y.Z): Clarifications, wording fixes, non-semantic updates.
3. **Run checklist**: Execute the Constitution Update Checklist:
   - Re-read all templates (`plan-template.md`, `spec-template.md`, `tasks-template.md`) and update references to changed principles.
   - Check all command files in `.specify/templates/commands/*.md` (if any) for stale references.
   - Update `docs/quickstart.md` or similar onboarding docs if principles affect developer workflow.
   - Verify all ADRs remain consistent with amended principles (mark conflicting ADRs as "Superseded").
4. **Review & approve**: At least one project maintainer must approve the PR.
5. **Merge & propagate**: After merge, update `LAST_AMENDED_DATE` to merge date.

### Compliance & Enforcement

- **PR gate**: All PRs MUST be reviewed against Constitution principles. Constitution violations require explicit justification (documented in PR description or ADR) or rework.
- **Complexity justification**: Introducing complexity beyond Constitution norms (e.g., new dependency, non-standard DSP technique, multi-threaded audio path) MUST be justified in the design doc and Complexity Tracking section of the feature plan.
- **Periodic review**: Project maintainers MUST review this Constitution at least annually and propose amendments if principles no longer reflect project reality.

---

**Version**: 1.0.0 | **Ratified**: 2025-11-03 | **Last Amended**: 2025-11-03
