# JNI API Contract: Kotlin ↔ C++ Interface

**Feature**: 001-edgeclear-av-vad
**Date**: 2025-11-03

## Overview

This document defines the JNI (Java Native Interface) contract between the Kotlin UI layer and C++ DSP core. The interface is kept minimal to reduce JNI overhead and maintain clear separation of concerns.

---

## Native Methods (Kotlin → C++)

### Session Management

#### `nativeInitSession`

```kotlin
external fun nativeInitSession(
    sampleRate: Int,           // 48000 Hz
    hopSize: Int,              // 160 samples @ 16 kHz
    preset: String             // "Low-latency" | "Quality" | "Battery saver"
): Long                        // Returns session handle (opaque pointer)
```

**Returns**: Session handle (C++ pointer cast to `jlong`), or 0 on failure.

**C++ Signature**:
```cpp
extern "C" JNIEXPORT jlong JNICALL
Java_com_edgeclear_audio_NativeAudioEngine_nativeInitSession(
    JNIEnv* env,
    jobject obj,
    jint sample_rate,
    jint hop_size,
    jstring preset
);
```

**Behavior**:
- Allocates all DSP state (STFT, AEC, denoiser).
- Loads TFLite model, verifies SHA-256.
- Initializes SPSC queues (4-6 hops based on preset).
- Returns handle to `AudioSession` C++ object.

**Error Handling**: Returns 0 on failure (e.g., model load fails, insufficient memory). Kotlin checks for 0 and displays error.

---

#### `nativeDestroySession`

```kotlin
external fun nativeDestroySession(sessionHandle: Long)
```

**C++ Signature**:
```cpp
extern "C" JNIEXPORT void JNICALL
Java_com_edgeclear_audio_NativeAudioEngine_nativeDestroySession(
    JNIEnv* env,
    jobject obj,
    jlong session_handle
);
```

**Behavior**:
- Deallocates all DSP state.
- Flushes any pending buffers.
- No-op if `session_handle == 0`.

---

### Component Control

#### `nativeSetComponentState`

```kotlin
external fun nativeSetComponentState(
    sessionHandle: Long,
    aecEnabled: Boolean,
    resEnabled: Boolean,
    denoiserEnabled: Boolean,
    avVadEnabled: Boolean
)
```

**C++ Signature**:
```cpp
extern "C" JNIEXPORT void JNICALL
Java_com_edgeclear_audio_NativeAudioEngine_nativeSetComponentState(
    JNIEnv* env,
    jobject obj,
    jlong session_handle,
    jboolean aec_enabled,
    jboolean res_enabled,
    jboolean denoiser_enabled,
    jboolean av_vad_enabled
);
```

**Behavior**:
- Updates component enable flags in session state (atomic stores with `memory_order_release`).
- Changes take effect on next DSP hop.
- Safe to call while audio running (lock-free atomic updates).

---

### Metrics Retrieval

#### `nativeGetMetrics`

```kotlin
data class NativeMetrics(
    val erle_dB: Float,
    val siSdr_dB: Float,
    val cpuMs: Float,
    val latencyMs: Float,
    val xrunCount: Int,
    val vadState: Int,      // 0 = INACTIVE, 1 = ACTIVE
    val dtdState: Int,      // 0 = SILENCE, 1 = NEAR_END_ONLY, 2 = FAR_END_ONLY, 3 = DOUBLE_TALK
    val avVadMode: String   // "active" | "fallback" | "disabled"
)

external fun nativeGetMetrics(sessionHandle: Long): NativeMetrics
```

**C++ Signature**:
```cpp
extern "C" JNIEXPORT jobject JNICALL
Java_com_edgeclear_audio_NativeAudioEngine_nativeGetMetrics(
    JNIEnv* env,
    jobject obj,
    jlong session_handle
);
```

**Behavior**:
- Reads latest metrics snapshot from session (atomic loads with `memory_order_acquire`).
- Constructs `NativeMetrics` Java object via JNI.
- Called every 1 second by Kotlin metrics poller (non-RT thread).

**Thread Safety**: Atomic reads, safe to call concurrently with audio processing.

---

### Recording Control

#### `nativeStartRecording`

```kotlin
external fun nativeStartRecording(
    sessionHandle: Long,
    rawPath: String,
    farPath: String,
    enhancedPath: String,
    durationSec: Int
): Boolean  // Returns true on success, false if already recording or insufficient storage
```

**C++ Signature**:
```cpp
extern "C" JNIEXPORT jboolean JNICALL
Java_com_edgeclear_audio_NativeAudioEngine_nativeStartRecording(
    JNIEnv* env,
    jobject obj,
    jlong session_handle,
    jstring raw_path,
    jstring far_path,
    jstring enhanced_path,
    jint duration_sec
);
```

**Behavior**:
- Opens three WAV files for writing (48 kHz, 16-bit PCM).
- Writes WAV headers with placeholder size (updated on stop).
- Starts recording state machine (see data-model.md).
- Returns `true` on success.

**Error Handling**: Returns `false` if already recording or file open fails (e.g., insufficient storage, permissions).

---

#### `nativeStopRecording`

```kotlin
external fun nativeStopRecording(sessionHandle: Long): Boolean  // Returns true if recording was active
```

**C++ Signature**:
```cpp
extern "C" JNIEXPORT jboolean JNICALL
Java_com_edgeclear_audio_NativeAudioEngine_nativeStopRecording(
    JNIEnv* env,
    jobject obj,
    jlong session_handle
);
```

**Behavior**:
- Closes WAV files, updates headers with final size.
- Writes metadata JSON (session config, device info).
- Returns `true` if recording was active, `false` otherwise.

---

## Callback Interface (C++ → Kotlin)

### Far-End Audio Playback

**Problem**: C++ DSP core needs far-end reference audio for AEC, but playback is controlled by Kotlin (AudioTrack).

**Solution**: Kotlin provides far-end samples via callback.

```kotlin
interface FarEndAudioProvider {
    fun getFarEndSamples(buffer: ShortArray): Int  // Returns number of samples written
}
```

**C++ calls this via JNI**:
```cpp
// Stored in session state during init
struct SessionState {
    JavaVM* jvm;
    jobject far_end_provider;  // Global ref to Kotlin FarEndAudioProvider
    jmethodID get_samples_method;
};

// In DSP worker thread
int get_far_end_samples(SessionState* session, int16_t* buffer, int num_samples) {
    JNIEnv* env;
    session->jvm->AttachCurrentThread(&env, nullptr);

    jshortArray j_buffer = env->NewShortArray(num_samples);
    jint n = env->CallIntMethod(session->far_end_provider, session->get_samples_method, j_buffer);
    env->GetShortArrayRegion(j_buffer, 0, n, buffer);
    env->DeleteLocalRef(j_buffer);

    session->jvm->DetachCurrentThread();
    return n;
}
```

**Thread Safety**: `AttachCurrentThread` / `DetachCurrentThread` on DSP worker thread (not RT audio callback). Acceptable latency (<1 ms).

---

## Data Type Marshalling

| Kotlin Type | C++ Type | Notes |
|-------------|----------|-------|
| `Int` | `jint` (`int32_t`) | Direct mapping |
| `Long` | `jlong` (`int64_t`) | Used for opaque pointers |
| `Float` | `jfloat` (`float`) | Direct mapping |
| `Boolean` | `jboolean` (`uint8_t`) | 0 = false, 1 = true |
| `String` | `jstring` → `const char*` | Use `GetStringUTFChars`, release with `ReleaseStringUTFChars` |
| `ShortArray` | `jshortArray` → `int16_t*` | Use `GetShortArrayElements`, release with `ReleaseShortArrayElements`, copy mode `JNI_ABORT` if read-only |

---

## Error Handling Strategy

**C++ → Kotlin**:
- **Success**: Return valid handle (non-zero) or `true`.
- **Failure**: Return 0 / `false`. Kotlin checks and displays user-friendly error message.
- **Critical errors**: Log to Android logcat (`__android_log_print`) with `ANDROID_LOG_ERROR` tag `"EdgeClear-Native"`.

**Kotlin → C++**:
- **Invalid handle**: C++ checks for null, returns early (no-op or default metrics).
- **JNI exceptions**: C++ checks `env->ExceptionCheck()` after JNI calls, clears and logs if exception occurred.

---

## Threading Model

**RT Audio Callback** (AAudio):
- **Thread**: OS-managed high-priority real-time thread.
- **JNI usage**: **NONE**. JNI calls forbidden in RT callback (unbounded latency).
- **Communication**: Via SPSC queues (lock-free, atomic operations only).

**DSP Worker Thread**:
- **Thread**: C++ `std::thread` created by session.
- **JNI usage**: Allowed (e.g., far-end audio callback). Attach/detach on first/last use.
- **Latency budget**: <1 ms per hop (non-RT, can tolerate occasional spikes).

**Kotlin UI Thread**:
- **Thread**: Android Main/UI thread.
- **JNI usage**: All session management and metrics retrieval calls originate here.
- **Safety**: All JNI calls documented as thread-safe (atomic operations in C++).

---

## Initialization Sequence

1. **Kotlin** creates `NativeAudioEngine` singleton.
2. **Kotlin** calls `System.loadLibrary("edgeclear-native")` → loads `libedgeclear-native.so`.
3. **Kotlin** calls `nativeInitSession(48000, 160, "Quality")` → returns session handle.
4. **Kotlin** registers `FarEndAudioProvider` callback (stores global ref in session).
5. **Kotlin** starts AAudio streams (capture + render).
6. **C++** RT callback starts pushing/popping SPSC queues.
7. **C++** DSP worker thread starts processing frames.

---

## Teardown Sequence

1. **Kotlin** stops AAudio streams.
2. **Kotlin** calls `nativeDestroySession(sessionHandle)`.
3. **C++** flushes SPSC queues, stops DSP worker thread.
4. **C++** deallocates all buffers, closes files, releases JNI global refs.
5. **Kotlin** sets session handle to 0 (invalid).

---

## Testing Strategy

**Unit Tests** (C++):
- Mock JNI environment (`test_jni_mock.cpp`).
- Validate marshalling correctness (int16 arrays, UTF strings).
- Validate handle lifecycle (init → use → destroy).

**Integration Tests** (Kotlin + C++):
- Use Robolectric to simulate Android environment.
- Call JNI methods with real `JNIEnv*`.
- Validate metrics snapshot accuracy.
- Validate recording file creation and metadata correctness.

---

## Next Steps

This contract feeds into:
1. **`jni_bridge.cpp`** implementation: Entry points for all native methods.
2. **`NativeAudioEngine.kt`** wrapper: Kotlin interface to JNI calls.
3. **Unit tests**: `test_jni_marshalling.cpp`, `NativeAudioEngineTest.kt`.
