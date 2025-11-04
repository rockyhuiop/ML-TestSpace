package com.edgeclear.viewmodel

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.edgeclear.audio.AudioSession
import com.edgeclear.audio.MetricsSnapshot
import com.edgeclear.audio.ProcessingPreset
import com.edgeclear.audio.Presets
import com.edgeclear.jni.NativeAudioEngine
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

/**
 * MonitorViewModel: Manages audio monitoring session and metrics
 *
 * Responsibilities:
 * - Initialize/destroy audio session
 * - Poll metrics every 1 second
 * - Update UI state
 * - Handle start/stop monitoring
 *
 * See data-model.md for AudioSession and MetricsSnapshot specifications.
 */
class MonitorViewModel(application: Application) : AndroidViewModel(application) {

    private val _session = MutableStateFlow<AudioSession?>(null)
    val session: StateFlow<AudioSession?> = _session.asStateFlow()

    private val _metrics = MutableStateFlow<MetricsSnapshot?>(null)
    val metrics: StateFlow<MetricsSnapshot?> = _metrics.asStateFlow()

    private val _isMonitoring = MutableStateFlow(false)
    val isMonitoring: StateFlow<Boolean> = _isMonitoring.asStateFlow()

    private val _errorMessage = MutableStateFlow<String?>(null)
    val errorMessage: StateFlow<String?> = _errorMessage.asStateFlow()

    // Phase 6: Recording lock state (shared with RecordViewModel)
    private val _isRecordingActive = MutableStateFlow(false)
    val isRecordingActive: StateFlow<Boolean> = _isRecordingActive.asStateFlow()

    private var sessionHandle: Long = 0

    /**
     * Start monitoring session.
     *
     * @param preset Processing preset (Low-latency, Quality, Battery saver)
     */
    fun startMonitoring(preset: ProcessingPreset = Presets.QUALITY) {
        if (_isMonitoring.value) {
            _errorMessage.value = "Already monitoring"
            return
        }

        viewModelScope.launch {
            // Initialize native session
            sessionHandle = NativeAudioEngine.nativeInitSession(
                sampleRate = 48000,
                hopSize = 160,  // 10 ms @ 16 kHz
                preset = preset.name,
                assetManager = getApplication<Application>().assets
            )

            if (sessionHandle == 0L) {
                _errorMessage.value = "Failed to initialize audio session"
                return@launch
            }

            // Create session object
            val newSession = AudioSession(currentPreset = preset)
            _session.value = newSession

            _isMonitoring.value = true
            _errorMessage.value = null

            // Start metrics polling (1 Hz)
            startMetricsPolling()
        }
    }

    /**
     * Stop monitoring session.
     */
    fun stopMonitoring() {
        if (!_isMonitoring.value) {
            return
        }

        _isMonitoring.value = false

        // Destroy native session
        if (sessionHandle != 0L) {
            NativeAudioEngine.nativeDestroySession(sessionHandle)
            sessionHandle = 0
        }

        _session.value = null
        _metrics.value = null
    }

    /**
     * Poll metrics every 1 second.
     */
    private fun startMetricsPolling() {
        viewModelScope.launch {
            while (isActive && _isMonitoring.value) {
                // Get metrics from native layer
                val nativeMetrics = NativeAudioEngine.nativeGetMetrics(sessionHandle)

                if (nativeMetrics != null) {
                    // Convert to Kotlin data class
                    val snapshot = MetricsSnapshot.fromNative(nativeMetrics)
                    _metrics.value = snapshot

                    // Update session duration
                    _session.value?.updateDuration()

                    // Update XRun count in session
                    _session.value?.xrunCount = snapshot.xrunCount
                }

                // Poll every 1 second
                delay(1000)
            }
        }
    }

    /**
     * Change processing preset (Phase 7 Task T095).
     *
     * Applies preset change to active session. Some preset changes may require
     * session restart if buffer sizing or AEC filter length differs.
     *
     * @param preset New processing preset
     */
    fun changePreset(preset: ProcessingPreset) {
        if (!_isMonitoring.value) {
            _errorMessage.value = "Cannot change preset: no active session"
            return
        }

        if (sessionHandle == 0L) {
            _errorMessage.value = "Invalid session"
            return
        }

        // Check if recording is active (should lock preset changes too)
        if (_isRecordingActive.value) {
            _errorMessage.value = "Cannot change preset during recording"
            return
        }

        val currentSession = _session.value
        if (currentSession == null) {
            _errorMessage.value = "No active session"
            return
        }

        // Check if preset is already active
        if (currentSession.currentPreset.name == preset.name) {
            _errorMessage.value = "Preset '${preset.name}' is already active"
            return
        }

        viewModelScope.launch {
            // Call native JNI method to apply preset
            val success = NativeAudioEngine.nativeSetPreset(sessionHandle, preset.name)

            if (success) {
                // Update session state
                _session.value = currentSession.copy(currentPreset = preset)
                _errorMessage.value = null
            } else {
                // Preset change failed (likely requires session restart)
                _errorMessage.value = "Preset change requires session restart. " +
                        "Stop monitoring and restart with '${preset.name}' preset."
            }
        }
    }

    /**
     * Clear error message.
     */
    fun clearError() {
        _errorMessage.value = null
    }

    /**
     * Phase 6 T080: Check if component toggles should be locked during recording.
     *
     * This method will be called by component toggle handlers (Phase 8) to prevent
     * changing AEC/RES/Denoiser/AV-VAD settings while recording is active.
     *
     * @return true if recording is active and toggles should be locked
     */
    fun isComponentToggleLocked(): Boolean {
        return _isRecordingActive.value
    }

    /**
     * Phase 6 T080: Set recording active state (called by RecordViewModel).
     *
     * This is a temporary solution for Phase 6. In production, this would be
     * managed via shared ViewModel or dependency injection.
     *
     * @param isRecording true if recording is active
     */
    fun setRecordingActive(isRecording: Boolean) {
        _isRecordingActive.value = isRecording
    }

    /**
     * Phase 6 T081: Get error message for component toggle attempt during recording.
     *
     * @return Error message if locked, null if allowed
     */
    fun onComponentToggleAttempt(): String? {
        return if (isComponentToggleLocked()) {
            "Cannot change components during recording"
        } else {
            null
        }
    }

    /**
     * Phase 8 T098: Toggle DSP component on/off.
     *
     * Updates component state in both Kotlin and native C++ layers.
     * Uses atomic bool stores with release ordering for thread-safe communication.
     *
     * @param componentName Component identifier: "aec", "res", "denoiser", "av_vad"
     * @param enabled New enable/disable state
     */
    fun toggleComponent(componentName: String, enabled: Boolean) {
        if (!_isMonitoring.value) {
            _errorMessage.value = "Cannot toggle component: no active session"
            return
        }

        if (sessionHandle == 0L) {
            _errorMessage.value = "Invalid session"
            return
        }

        // Check recording lock (should already be prevented by UI, but double-check)
        if (_isRecordingActive.value) {
            _errorMessage.value = "Cannot change components during recording"
            return
        }

        val currentSession = _session.value
        if (currentSession == null) {
            _errorMessage.value = "No active session"
            return
        }

        viewModelScope.launch {
            // Call native JNI method to update component state atomically
            val success = NativeAudioEngine.nativeSetComponentState(
                sessionHandle,
                componentName,
                enabled
            )

            if (success) {
                // Update local session state
                val updatedComponentState = when (componentName) {
                    "aec" -> currentSession.componentState.copy(aecEnabled = enabled)
                    "res" -> currentSession.componentState.copy(resEnabled = enabled)
                    "denoiser" -> currentSession.componentState.copy(denoiserEnabled = enabled)
                    "av_vad" -> currentSession.componentState.copy(avVadEnabled = enabled)
                    else -> currentSession.componentState
                }

                _session.value = currentSession.copy(componentState = updatedComponentState)
                _errorMessage.value = null
            } else {
                _errorMessage.value = "Failed to toggle component: $componentName"
            }
        }
    }

    override fun onCleared() {
        super.onCleared()
        stopMonitoring()
    }
}
