package com.edgeclear.viewmodel

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.edgeclear.audio.ABRecording
import com.edgeclear.audio.ComponentState
import com.edgeclear.audio.StorageManager
import com.edgeclear.jni.NativeAudioEngine
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import java.util.UUID

/**
 * ViewModel for RecordScreen - manages A/B recording state and interactions
 *
 * Responsibilities:
 * - Start/stop recording via JNI bridge
 * - Track recording duration with auto-stop
 * - Validate storage availability before recording
 * - Manage recording library (list, delete, share)
 * - Enforce component toggle lock during recording
 */
class RecordViewModel(application: Application) : AndroidViewModel(application) {

    private val nativeEngine = NativeAudioEngine
    private val storageManager = StorageManager(application)

    // Recording state
    private val _recordingState = MutableStateFlow<RecordingState>(RecordingState.Idle)
    val recordingState: StateFlow<RecordingState> = _recordingState.asStateFlow()

    // Current recording
    private val _currentRecording = MutableStateFlow<ABRecording?>(null)
    val currentRecording: StateFlow<ABRecording?> = _currentRecording.asStateFlow()

    // Recording duration (seconds elapsed)
    private val _elapsedSeconds = MutableStateFlow(0)
    val elapsedSeconds: StateFlow<Int> = _elapsedSeconds.asStateFlow()

    // Selected duration (seconds)
    private val _selectedDuration = MutableStateFlow(15)
    val selectedDuration: StateFlow<Int> = _selectedDuration.asStateFlow()

    // Recording library
    private val _recordings = MutableStateFlow<List<ABRecording>>(emptyList())
    val recordings: StateFlow<List<ABRecording>> = _recordings.asStateFlow()

    // Error messages
    private val _errorMessage = MutableStateFlow<String?>(null)
    val errorMessage: StateFlow<String?> = _errorMessage.asStateFlow()

    // Storage info
    private val _storageInfo = MutableStateFlow("")
    val storageInfo: StateFlow<String> = _storageInfo.asStateFlow()

    // Timer job
    private var timerJob: Job? = null

    init {
        updateStorageInfo()
        loadRecordings()
    }

    /**
     * Sets the desired recording duration (15, 30, or 60 seconds)
     */
    fun setDuration(durationSeconds: Int) {
        if (_recordingState.value == RecordingState.Idle) {
            _selectedDuration.value = durationSeconds
        }
    }

    /**
     * Starts a new A/B recording session
     *
     * @param componentStates Current component states (for metadata)
     * @return true if recording started successfully, false otherwise
     */
    fun startRecording(componentStates: ComponentState): Boolean {
        // Check if already recording
        if (_recordingState.value != RecordingState.Idle) {
            _errorMessage.value = "Recording already in progress"
            return false
        }

        // Check storage availability
        val duration = _selectedDuration.value
        val (isAvailable, storageError) = storageManager.checkStorageAvailability(duration)
        if (!isAvailable) {
            _errorMessage.value = storageError
            _recordingState.value = RecordingState.Error(storageError)
            return false
        }

        // Create recording metadata
        val sessionId = UUID.randomUUID().toString()
        val recording = ABRecording.create(
            sessionId = sessionId,
            baseDir = storageManager.getRecordingsDirectory(),
            durationSeconds = duration,
            componentStates = componentStates
        )

        // Start native recording
        val success = nativeEngine.startRecording(
            rawPath = recording.rawFilePath,
            farEndPath = recording.farEndFilePath,
            enhancedPath = recording.enhancedFilePath,
            metadataPath = recording.metadataFilePath
        )

        if (!success) {
            _errorMessage.value = "Failed to start native recording"
            _recordingState.value = RecordingState.Error("Native recording failed")
            return false
        }

        // Update state
        _currentRecording.value = recording
        _recordingState.value = RecordingState.Recording
        _elapsedSeconds.value = 0
        _errorMessage.value = null

        // Start timer
        startTimer(duration)

        return true
    }

    /**
     * Stops the current recording session
     */
    fun stopRecording() {
        if (_recordingState.value != RecordingState.Recording) {
            return
        }

        // Stop timer
        timerJob?.cancel()
        timerJob = null

        // Stop native recording
        val success = nativeEngine.stopRecording()

        if (success) {
            _recordingState.value = RecordingState.Completed

            // Add to library
            _currentRecording.value?.let { recording ->
                _recordings.value = listOf(recording) + _recordings.value
            }

            // Update storage info
            updateStorageInfo()
        } else {
            _errorMessage.value = "Failed to stop recording"
            _recordingState.value = RecordingState.Error("Failed to stop recording")
        }

        // Clear current recording after short delay (to show completion message)
        viewModelScope.launch {
            delay(2000)
            if (_recordingState.value == RecordingState.Completed) {
                _recordingState.value = RecordingState.Idle
                _currentRecording.value = null
                _elapsedSeconds.value = 0
            }
        }
    }

    /**
     * Starts the recording duration timer with auto-stop
     */
    private fun startTimer(maxDurationSeconds: Int) {
        timerJob = viewModelScope.launch {
            for (elapsed in 1..maxDurationSeconds) {
                delay(1000) // 1 second
                _elapsedSeconds.value = elapsed

                // Auto-stop when max duration reached
                if (elapsed >= maxDurationSeconds) {
                    stopRecording()
                    break
                }
            }
        }
    }

    /**
     * Loads existing recordings from storage
     */
    fun loadRecordings() {
        viewModelScope.launch {
            // TODO: Implement proper recording deserialization from metadata files
            // For now, just clear the list
            _recordings.value = emptyList()
        }
    }

    /**
     * Deletes a recording from storage
     */
    fun deleteRecording(recording: ABRecording) {
        if (recording.deleteFiles()) {
            _recordings.value = _recordings.value.filter { it.sessionId != recording.sessionId }
            updateStorageInfo()
        } else {
            _errorMessage.value = "Failed to delete recording"
        }
    }

    /**
     * Updates storage info string
     */
    private fun updateStorageInfo() {
        _storageInfo.value = storageManager.getStorageInfo()
    }

    /**
     * Clears error message
     */
    fun clearError() {
        _errorMessage.value = null
        if (_recordingState.value is RecordingState.Error) {
            _recordingState.value = RecordingState.Idle
        }
    }

    /**
     * Returns whether component toggles should be locked
     */
    fun isComponentToggleLocked(): Boolean {
        return _recordingState.value == RecordingState.Recording
    }

    /**
     * Returns formatted time string (MM:SS)
     */
    fun formatTime(seconds: Int): String {
        val mins = seconds / 60
        val secs = seconds % 60
        return String.format("%02d:%02d", mins, secs)
    }

    override fun onCleared() {
        super.onCleared()
        timerJob?.cancel()
    }
}

/**
 * Recording state sealed class
 */
sealed class RecordingState {
    object Idle : RecordingState()
    object Recording : RecordingState()
    object Completed : RecordingState()
    data class Error(val message: String) : RecordingState()
}
