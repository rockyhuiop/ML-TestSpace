package com.edgeclear.audio

/**
 * ComponentState: Enable/disable flags for each DSP component
 *
 * Lifecycle: Created with session, updated via user toggles.
 * Locked during A/B recording (FR-019d).
 *
 * See data-model.md for state machine and validation rules.
 */
data class ComponentState(
    var aecEnabled: Boolean = true,
    var resEnabled: Boolean = true,
    var denoiserEnabled: Boolean = true,
    var avVadEnabled: Boolean = false,
    var isLocked: Boolean = false
) {
    /**
     * Attempt to toggle a component.
     *
     * @return true if toggle succeeded, false if locked
     */
    fun toggleAEC(): Boolean {
        if (isLocked) return false
        aecEnabled = !aecEnabled
        return true
    }

    fun toggleRES(): Boolean {
        if (isLocked) return false
        resEnabled = !resEnabled
        return true
    }

    fun toggleDenoiser(): Boolean {
        if (isLocked) return false
        denoiserEnabled = !denoiserEnabled
        return true
    }

    fun toggleAVVad(): Boolean {
        if (isLocked) return false
        avVadEnabled = !avVadEnabled
        return true
    }

    /**
     * Lock components (e.g., when recording starts)
     */
    fun lock() {
        isLocked = true
    }

    /**
     * Unlock components (e.g., when recording stops)
     */
    fun unlock() {
        isLocked = false
    }
}
