package com.edgeclear.viewmodel

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.edgeclear.audio.AudioSession
import com.edgeclear.audio.MetricsSnapshot
import com.edgeclear.audio.ProcessingPreset
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

    private var sessionHandle: Long = 0

    /**
     * Start monitoring session.
     *
     * @param preset Processing preset (Low-latency, Quality, Battery saver)
     */
    fun startMonitoring(preset: ProcessingPreset = ProcessingPreset.QUALITY) {
        if (_isMonitoring.value) {
            _errorMessage.value = "Already monitoring"
            return
        }

        viewModelScope.launch {
            // Initialize native session
            sessionHandle = NativeAudioEngine.nativeInitSession(
                sampleRate = 48000,
                hopSize = 160,  // 10 ms @ 16 kHz
                preset = preset.displayName,
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
     * Change processing preset.
     */
    fun changePreset(preset: ProcessingPreset) {
        // TODO: Implement dynamic preset change via JNI (Phase 7)
        // For now, requires restart
        _errorMessage.value = "Preset change requires restart (implement in Phase 7)"
    }

    /**
     * Clear error message.
     */
    fun clearError() {
        _errorMessage.value = null
    }

    override fun onCleared() {
        super.onCleared()
        stopMonitoring()
    }
}
