package com.edgeclear.jni

import android.content.res.AssetManager

/**
 * Native Metrics data class - matches JNI contract
 */
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

/**
 * NativeAudioEngine: JNI wrapper for C++ DSP core
 *
 * Singleton providing access to native audio processing functions.
 * See contracts/jni-api.md for full API specification.
 */
object NativeAudioEngine {

    init {
        // Load native library
        System.loadLibrary("edgeclear-native")
    }

    // Session Management

    external fun nativeInitSession(
        sampleRate: Int,       // 48000 Hz
        hopSize: Int,          // 160 samples @ 16 kHz
        preset: String,        // "Low-latency" | "Quality" | "Battery saver"
        assetManager: AssetManager  // For loading model from assets
    ): Long                    // Returns session handle, or 0 on failure

    external fun nativeDestroySession(sessionHandle: Long)

    // Component Control

    external fun nativeSetComponentState(
        sessionHandle: Long,
        aecEnabled: Boolean,
        resEnabled: Boolean,
        denoiserEnabled: Boolean,
        avVadEnabled: Boolean
    )

    // Metrics Retrieval

    external fun nativeGetMetrics(sessionHandle: Long): NativeMetrics?

    // Recording Control

    external fun nativeStartRecording(
        sessionHandle: Long,
        rawPath: String,
        farPath: String,
        enhancedPath: String,
        metadataPath: String
    ): Boolean  // Returns true on success

    external fun nativeStopRecording(sessionHandle: Long): Boolean

    // Convenience wrappers (using managed session handle)
    private var currentSessionHandle: Long = 0L

    fun setSessionHandle(handle: Long) {
        currentSessionHandle = handle
    }

    fun startRecording(
        rawPath: String,
        farEndPath: String,
        enhancedPath: String,
        metadataPath: String
    ): Boolean {
        return if (currentSessionHandle != 0L) {
            nativeStartRecording(currentSessionHandle, rawPath, farEndPath, enhancedPath, metadataPath)
        } else {
            false
        }
    }

    fun stopRecording(): Boolean {
        return if (currentSessionHandle != 0L) {
            nativeStopRecording(currentSessionHandle)
        } else {
            false
        }
    }
}
