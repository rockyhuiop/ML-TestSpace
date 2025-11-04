/**
 * jni_bridge.cpp - JNI interface between Kotlin and C++ DSP core
 *
 * Entry points for session management, component control, metrics retrieval.
 * See contracts/jni-api.md for full API specification.
 */

#include <jni.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <cstdint>
#include <cstring>

#include "pipeline/session_manager.h"

#define LOG_TAG "EdgeClear-Native"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using namespace edgeclear;

extern "C" {

/**
 * Initialize audio processing session.
 *
 * @return Session handle (opaque pointer cast to jlong), or 0 on failure
 */
JNIEXPORT jlong JNICALL
Java_com_edgeclear_jni_NativeAudioEngine_nativeInitSession(
        JNIEnv* env,
        jobject /* this */,
        jint sample_rate,
        jint hop_size,
        jstring preset,
        jobject asset_manager_obj) {

    const char* preset_str = env->GetStringUTFChars(preset, nullptr);
    LOGI("nativeInitSession: sample_rate=%d, hop_size=%d, preset=%s",
         sample_rate, hop_size, preset_str);

    // Get native AssetManager from Java AssetManager object
    AAssetManager* assetManager = nullptr;
    if (asset_manager_obj) {
        assetManager = AAssetManager_fromJava(env, asset_manager_obj);
    }

    // Create session manager
    auto* session = new pipeline::SessionManager();

    // Initialize session
    if (!session->Initialize(sample_rate, hop_size, std::string(preset_str), assetManager)) {
        LOGE("Failed to initialize session");
        delete session;
        env->ReleaseStringUTFChars(preset, preset_str);
        return 0;
    }

    // Start audio processing
    if (!session->Start()) {
        LOGE("Failed to start session");
        delete session;
        env->ReleaseStringUTFChars(preset, preset_str);
        return 0;
    }

    env->ReleaseStringUTFChars(preset, preset_str);

    LOGI("Session initialized and started successfully");
    return reinterpret_cast<jlong>(session);
}

/**
 * Destroy audio processing session.
 */
JNIEXPORT void JNICALL
Java_com_edgeclear_jni_NativeAudioEngine_nativeDestroySession(
        JNIEnv* env,
        jobject /* this */,
        jlong session_handle) {
    (void)env;  // Unused parameter

    LOGI("nativeDestroySession: handle=%lld", static_cast<long long>(session_handle));

    if (session_handle == 0) {
        return;
    }

    auto* session = reinterpret_cast<pipeline::SessionManager*>(session_handle);
    session->Destroy();
    delete session;

    LOGI("Session destroyed");
}

/**
 * Set component enable/disable state.
 */
JNIEXPORT void JNICALL
Java_com_edgeclear_jni_NativeAudioEngine_nativeSetComponentState(
        JNIEnv* env,
        jobject /* this */,
        jlong session_handle,
        jboolean aec_enabled,
        jboolean res_enabled,
        jboolean denoiser_enabled,
        jboolean av_vad_enabled) {
    (void)env;  // Unused parameter
    (void)session_handle;  // Unused in stub implementation

    LOGI("nativeSetComponentState: AEC=%d RES=%d Denoiser=%d AV-VAD=%d",
         aec_enabled, res_enabled, denoiser_enabled, av_vad_enabled);

    // TODO: Update component flags in session state (atomic stores)
}

/**
 * Get current metrics snapshot.
 *
 * @return NativeMetrics object
 */
JNIEXPORT jobject JNICALL
Java_com_edgeclear_jni_NativeAudioEngine_nativeGetMetrics(
        JNIEnv* env,
        jobject /* this */,
        jlong session_handle) {

    if (session_handle == 0) {
        return nullptr;
    }

    auto* session = reinterpret_cast<pipeline::SessionManager*>(session_handle);
    auto* metrics = session->GetMetrics();

    if (!metrics) {
        LOGE("Metrics not available");
        return nullptr;
    }

    // Read metrics from accumulator (atomic loads)
    const float erle_db = metrics->GetERLE();
    const float siSdr_db = metrics->GetSISDR();
    const float cpu_ms = metrics->GetCPU();
    const float latency_ms = metrics->GetLatency();
    const int xrun_count = static_cast<int>(metrics->GetXRunCount());
    const int vad_state = metrics->GetVADState();
    const int dtd_state = metrics->GetDTDState();

    // Find NativeMetrics class
    jclass metricsClass = env->FindClass("com/edgeclear/jni/NativeMetrics");
    if (!metricsClass) {
        LOGE("Failed to find NativeMetrics class");
        return nullptr;
    }

    // Find constructor: NativeMetrics(Float, Float, Float, Float, Int, Int, Int, String)
    jmethodID constructor = env->GetMethodID(metricsClass, "<init>",
                                            "(FFFFIIILjava/lang/String;)V");
    if (!constructor) {
        LOGE("Failed to find NativeMetrics constructor");
        return nullptr;
    }

    // Create NativeMetrics object with real data
    jstring avVadMode = env->NewStringUTF("disabled");  // TODO: Implement AV-VAD in Phase 9
    jobject metricsObj = env->NewObject(metricsClass, constructor,
                                        erle_db,
                                        siSdr_db,
                                        cpu_ms,
                                        latency_ms,
                                        xrun_count,
                                        vad_state,
                                        dtd_state,
                                        avVadMode);

    env->DeleteLocalRef(avVadMode);
    return metricsObj;
}

/**
 * Start A/B recording.
 *
 * @return true on success, false if already recording or error
 */
JNIEXPORT jboolean JNICALL
Java_com_edgeclear_jni_NativeAudioEngine_nativeStartRecording(
        JNIEnv* env,
        jobject /* this */,
        jlong session_handle,
        jstring raw_path,
        jstring far_path,
        jstring enhanced_path,
        jstring metadata_path) {

    if (session_handle == 0) {
        LOGE("Invalid session handle");
        return JNI_FALSE;
    }

    // Convert jstrings to C++ strings
    const char* raw_path_str = env->GetStringUTFChars(raw_path, nullptr);
    const char* far_path_str = env->GetStringUTFChars(far_path, nullptr);
    const char* enhanced_path_str = env->GetStringUTFChars(enhanced_path, nullptr);
    const char* metadata_path_str = env->GetStringUTFChars(metadata_path, nullptr);

    LOGI("nativeStartRecording");
    LOGI("  Raw: %s", raw_path_str);
    LOGI("  Far-end: %s", far_path_str);
    LOGI("  Enhanced: %s", enhanced_path_str);
    LOGI("  Metadata: %s", metadata_path_str);

    auto* session = reinterpret_cast<pipeline::SessionManager*>(session_handle);
    bool success = session->StartRecording(
        std::string(raw_path_str),
        std::string(far_path_str),
        std::string(enhanced_path_str),
        std::string(metadata_path_str)
    );

    // Release strings
    env->ReleaseStringUTFChars(raw_path, raw_path_str);
    env->ReleaseStringUTFChars(far_path, far_path_str);
    env->ReleaseStringUTFChars(enhanced_path, enhanced_path_str);
    env->ReleaseStringUTFChars(metadata_path, metadata_path_str);

    return success ? JNI_TRUE : JNI_FALSE;
}

/**
 * Stop A/B recording.
 *
 * @return true if recording was active
 */
JNIEXPORT jboolean JNICALL
Java_com_edgeclear_jni_NativeAudioEngine_nativeStopRecording(
        JNIEnv* env,
        jobject /* this */,
        jlong session_handle) {
    (void)env;  // Unused parameter

    if (session_handle == 0) {
        LOGE("Invalid session handle");
        return JNI_FALSE;
    }

    LOGI("nativeStopRecording");

    auto* session = reinterpret_cast<pipeline::SessionManager*>(session_handle);
    bool success = session->StopRecording();

    return success ? JNI_TRUE : JNI_FALSE;
}

/**
 * Set processing preset.
 *
 * Updates DSP pipeline configuration atomically:
 * - Buffer sizing (hop count)
 * - AEC filter length (partitions)
 * - Denoiser frame rate decimation
 * - RES enable/disable
 * - Camera frame rate (for AV-VAD)
 *
 * @param preset_name Preset name: "Low-latency", "Quality", or "Battery saver"
 * @return true on success, false on invalid preset or session error
 */
JNIEXPORT jboolean JNICALL
Java_com_edgeclear_jni_NativeAudioEngine_nativeSetPreset(
        JNIEnv* env,
        jobject /* this */,
        jlong session_handle,
        jstring preset_name) {

    if (session_handle == 0) {
        LOGE("Invalid session handle");
        return JNI_FALSE;
    }

    const char* preset_str = env->GetStringUTFChars(preset_name, nullptr);
    LOGI("nativeSetPreset: preset=%s", preset_str);

    auto* session = reinterpret_cast<pipeline::SessionManager*>(session_handle);
    bool success = session->SetPreset(std::string(preset_str));

    env->ReleaseStringUTFChars(preset_name, preset_str);

    if (!success) {
        LOGE("Failed to set preset: %s", preset_str);
        return JNI_FALSE;
    }

    LOGI("Preset applied successfully: %s", preset_str);
    return JNI_TRUE;
}

}  // extern "C"
