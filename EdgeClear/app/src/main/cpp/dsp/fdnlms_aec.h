#pragma once
// AEC stub - implement in Phase 4 M2
namespace edgeclear { namespace dsp {
class FDNLMS_AEC {
public:
    void ProcessFrame(const float* near, const float* far, float* out, int size) {
        // Stub: pass through near signal
        for (int i = 0; i < size; ++i) out[i] = near[i];
    }
};
}}
