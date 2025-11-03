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
        jint duration_sec) {

    LOGI("nativeStartRecording: duration=%d sec", duration_sec);

    // TODO: Open WAV files, start recording state machine
    return JNI_TRUE;  // Stub: always succeed
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

    LOGI("nativeStopRecording");

    // TODO: Close WAV files, write metadata JSON
    return JNI_FALSE;  // Stub: no recording was active
}

}  // extern "C"
