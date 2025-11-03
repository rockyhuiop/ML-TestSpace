// cola_windows.cpp - Implementation file
#include "cola_windows.h"

// All implementation is in header (static methods)
namespace edgeclear {
namespace dsp {

// Explicit instantiation to verify compilation
static float g_test_window[COLAWindows::kWindowSize];
static bool g_cola_validated = false;

void __attribute__((constructor)) ValidateCOLAOnLoad() {
    // Validate COLA property at library load time
    COLAWindows::GenerateSqrtHann(g_test_window);
    const float max_dev = COLAWindows::ValidateCOLA(g_test_window);

    constexpr float kLsbThreshold = 0.5f / 32768.0f;
    g_cola_validated = (max_dev < kLsbThreshold);

    // Note: In production, log validation result
    // For now, silent pass/fail (logged via Android logcat in jni_bridge)
}

}  // namespace dsp
}  // namespace edgeclear
