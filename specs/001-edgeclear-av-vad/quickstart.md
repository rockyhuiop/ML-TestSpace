# EdgeClear Developer Quickstart

**Feature**: 001-edgeclear-av-vad
**Date**: 2025-11-03
**Target Audience**: Developers setting up EdgeClear build environment

## Prerequisites

### Required Software

| Tool | Version | Purpose | Install Command |
|------|---------|---------|-----------------|
| **Android Studio** | Hedgehog (2023.1.1+) | IDE + Android SDK | [Download](https://developer.android.com/studio) |
| **Android NDK** | r26 | C++ toolchain | Android Studio SDK Manager → NDK (Side by side) → select 26.x |
| **CMake** | 3.22+ | Native build system | Android Studio SDK Manager → CMake → select 3.22+ |
| **Git** | 2.30+ | Version control | `brew install git` (macOS) / `apt install git` (Linux) |
| **Python** | 3.9+ | Test scripts | `brew install python3` / `apt install python3` |

### Recommended (Optional)

| Tool | Purpose |
|------|---------|
| **Android Emulator** (API 31+) | Testing without physical device |
| **Simpleperf** | CPU profiling (bundled with NDK) |
| **clang-tidy** | Static analysis (bundled with NDK) |

---

## Environment Setup

### 1. Clone Repository

```bash
git clone https://github.com/your-org/EdgeClear.git
cd EdgeClear
```

### 2. Configure NDK Path

Edit `gradle.properties`:

```properties
# Pin NDK version (Constitution requirement)
android.ndkVersion=26.1.10909125

# Optional: specify custom NDK path if not using SDK Manager
# android.ndkPath=/path/to/ndk/26.1.10909125
```

Verify NDK installation:

```bash
# macOS/Linux
ls $ANDROID_SDK_ROOT/ndk/26.1.10909125

# Expected output: build/ meta/ NOTICE prebuilt/ python-packages/ shader-tools/ simpleperf/ source.properties sources/ toolchains/ wrap.sh
```

### 3. Sync Gradle Dependencies

```bash
./gradlew --refresh-dependencies
```

**Expected output**: `BUILD SUCCESSFUL` within 2-3 minutes (first run downloads dependencies).

---

## Building the Project

### Debug Build (Default)

```bash
./gradlew assembleDebug
```

**Output**: `app/build/outputs/apk/debug/app-debug.apk`

**Build time**: ~90 seconds (first build), ~15 seconds (incremental).

**Configuration**:
- C++ optimizations: `-O0` (no optimization, debuggable)
- Sanitizers: ASan enabled (see CMake presets)
- Symbols: Included (native stack traces in logcat)

### Release Build

```bash
./gradlew assembleRelease
```

**Output**: `app/build/outputs/apk/release/app-release.apk`

**Build time**: ~120 seconds (includes optimization, stripping).

**Configuration**:
- C++ optimizations: `-O3 -DNDEBUG`
- Sanitizers: Disabled
- Symbols: Stripped (use `ndk-stack` for crash analysis)

### CMake Presets

Build specific configurations:

```bash
# Debug with ASan (Address Sanitizer)
cmake --preset debug-asan
cmake --build build/debug-asan

# Debug with TSan (Thread Sanitizer)
cmake --preset debug-tsan
cmake --build build/debug-tsan

# Release (optimized, no sanitizers)
cmake --preset release
cmake --build build/release
```

**Note**: Gradle invokes CMake automatically. Manual CMake builds useful for native-only development/testing.

---

## Running Tests

### Kotlin Unit Tests (JUnit)

```bash
./gradlew testDebugUnitTest
```

**Coverage**: UI logic, ViewModel state management, JNI marshalling mocks.

**Report**: `app/build/reports/tests/testDebugUnitTest/index.html`

### C++ Unit Tests (Google Test)

```bash
# Build C++ test binary
cd app/src/main/cpp
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
make -j8

# Run tests
./test_runner
```

**Coverage**: DSP modules (STFT, AEC, denoiser), SPSC queue, ring buffer.

**Expected output**:
```
[==========] Running 47 tests from 8 test suites.
[  PASSED  ] 47 tests.
```

### Golden Clip Validation

```bash
cd tests/integration
./run_golden_clips.sh
```

**Validates**:
- PESQ, STOI, SI-SDR, ERLE on standardized test clips.
- CPU usage per hop (<6 ms).
- End-to-end latency (<40 ms).

**Report**: `tests/integration/results/golden_clip_report.html`

**Expected duration**: ~5 minutes (processes 10 clips × 3 configs).

### 10-Minute Soak Test

```bash
cd tests/integration
python3 soak_test.py --duration 600 --device <DEVICE_SERIAL>
```

**Validates**:
- Zero XRuns over 10-minute run.
- Stable CPU/latency (no drift).
- No crashes or memory leaks.

**Report**: `tests/integration/results/soak_test_<timestamp>.json`

---

## Installing & Running on Device

### 1. Enable Developer Options

**On Android device**:
1. Settings → About Phone → tap "Build number" 7 times.
2. Settings → Developer Options → enable "USB debugging".

### 2. Connect Device

```bash
adb devices
```

**Expected output**:
```
List of devices attached
1234567890ABCDEF    device
```

If "unauthorized", check device screen for USB debugging prompt.

### 3. Install APK

```bash
./gradlew installDebug
```

**Alternative** (manual install):
```bash
adb install app/build/outputs/apk/debug/app-debug.apk
```

### 4. Launch App

```bash
adb shell am start -n com.edgeclear/.MainActivity
```

**Alternative**: Tap "EdgeClear" icon on device.

---

## Debugging

### Logcat (Native + Kotlin Logs)

```bash
adb logcat -s EdgeClear:V EdgeClear-Native:V
```

**Log tags**:
- `EdgeClear`: Kotlin UI logs.
- `EdgeClear-Native`: C++ DSP logs.

**Example output**:
```
D/EdgeClear-Native: [AEC] ERLE = 15.3 dB, DTD = NEAR_END_ONLY
D/EdgeClear: MetricsSnapshot: CPU=4.2ms, Latency=36ms, XRuns=0
```

### Native Stack Traces (Crashes)

If native crash occurs, logcat shows:

```
F/libc    : Fatal signal 11 (SIGSEGV), code 1 (SEGV_MAPERR), fault addr 0x0 in tid 12345 (DSP-Worker), pid 67890 (com.edgeclear)
```

**Symbolicate**:

```bash
# Extract tombstone
adb pull /data/tombstones/tombstone_XX

# Symbolicate (use debug APK symbols)
ndk-stack -sym app/build/intermediates/cmake/debug/obj/arm64-v8a -dump tombstone_XX
```

**Output**: Readable stack trace with function names and line numbers.

### CPU Profiling (Simpleperf)

**Record**:
```bash
adb shell simpleperf record -p $(adb shell pidof com.edgeclear) -o /data/local/tmp/perf.data
# Let run for 10 seconds
adb shell simpleperf stat -p $(adb shell pidof com.edgeclear)
```

**Analyze**:
```bash
adb pull /data/local/tmp/perf.data
simpleperf report -i perf.data
```

**Output**: Hotspot functions ranked by CPU time (identify bottlenecks).

### Memory Leaks (ASan)

Build with ASan preset:
```bash
./gradlew assembleDebug -Pandroid.debug.sanitizers=ASAN
adb install app/build/outputs/apk/debug/app-debug.apk
```

Run app, trigger leak scenario, check logcat for:
```
==12345==ERROR: LeakSanitizer: detected memory leaks
Direct leak of 1024 byte(s) in 1 object(s) allocated from:
    #0 0x... in malloc
    #1 0x... in allocate_buffer src/dsp/stft.cpp:45
```

---

## Performance Validation

### Latency Measurement

**Built-in tool** (impulse loopback):

1. Launch app → Monitor screen.
2. Tap "Measure Latency" button.
3. App plays impulse, records loopback, displays result.

**Expected**: 36-38 ms on SD 778G (Quality preset).

**Manual verification** (external loopback cable):

1. Connect headphone jack to microphone input (loopback cable).
2. Run golden clip suite with loopback enabled.
3. Check `results/latency_loopback.txt`.

### CPU Budget Validation

**Real-time metrics** (Monitor screen):

- CPU ms/hop displayed live (updated every 1 second).
- Red indicator if >6 ms (budget violation).

**Golden clip validation**:

```bash
./run_golden_clips.sh --profile-cpu
```

**Report**: `results/cpu_profile.csv` (per-hop timing breakdown: STFT, AEC, denoiser, ISTFT).

### Memory Usage

**Android Studio Profiler**:

1. Run → Profile 'app'.
2. Select device and app.
3. Memory tab → monitor heap usage.
4. Run 10-minute soak test → verify peak <128 MB.

**adb meminfo**:
```bash
adb shell dumpsys meminfo com.edgeclear
```

**Key metric**: `TOTAL PSS` should be <128 MB during active session.

---

## Troubleshooting

### Build Failures

**Symptom**: `CMake Error: Could not find NDK`

**Fix**: Ensure NDK r26 installed via SDK Manager, verify `android.ndkVersion` in `gradle.properties`.

---

**Symptom**: `clang: error: unsupported option '-fsanitize=address'`

**Fix**: ASan requires NDK r21+. Verify NDK version ≥26.

---

**Symptom**: `error: 'std::span' not found`

**Fix**: C++20 required. Check `CMakeLists.txt` has `set(CMAKE_CXX_STANDARD 20)`.

---

### Runtime Issues

**Symptom**: App crashes on launch with "Library not found: libedgeclear-native.so"

**Fix**: Clean and rebuild: `./gradlew clean assembleDebug`. Verify APK contains `lib/arm64-v8a/libedgeclear-native.so`.

---

**Symptom**: XRuns reported, audio dropouts

**Fix**:
1. Check CPU usage (should be <6 ms/hop). If exceeded, switch to Battery Saver preset.
2. Check device is not thermal throttling (run `adb shell cat /sys/class/thermal/thermal_zone*/temp`).
3. Close background apps consuming CPU.

---

**Symptom**: AV-VAD always shows "fallback"

**Fix**:
1. Grant camera permission: Settings → Apps → EdgeClear → Permissions → Camera → Allow.
2. Ensure face visible to front camera (good lighting, no occlusion).
3. Check logcat for face detection errors (`EdgeClear-Native: [AV-VAD] Face detection failed`).

---

## Next Steps

1. **Implement M1** (Core Audio Loop): Follow `tasks.md` after running `/speckit.tasks`.
2. **Run unit tests** after each module implementation.
3. **Profile CPU** after AEC integration (M2) to validate budget.
4. **Golden clip validation** after denoiser integration (M3).
5. **10-minute soak test** before each milestone sign-off.

## Resources

- **Constitution**: `EdgeClear/.specify/memory/constitution.md` (non-negotiables, quality bars)
- **Design Docs**: `EdgeClear/docs/design/` (module-specific deep dives)
- **ADRs**: `EdgeClear/docs/adr/` (architecture decisions with rationale)
- **Spec**: `specs/001-edgeclear-av-vad/spec.md` (user stories, requirements)
- **Plan**: `specs/001-edgeclear-av-vad/plan.md` (architecture, milestones)

**Questions?** File issue on GitHub or check `docs/faq.md`.
