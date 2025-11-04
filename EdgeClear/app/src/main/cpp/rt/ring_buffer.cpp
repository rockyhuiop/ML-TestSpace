// ring_buffer.cpp - Implementation file
// SPSCRingBuffer is template class (header-only)
#include "ring_buffer.h"
#include "frame_buffer.h"

// Explicit template instantiations for common types
namespace edgeclear {
namespace rt {

// Instantiate for DSPFrameBuffer (most common use case)
template class SPSCRingBuffer<DSPFrameBuffer>;

}  // namespace rt
}  // namespace edgeclear
// Phase 7 Task T090: Dynamic buffer resizing notes
//
// Dynamic resizing of SPSCRingBuffer during active session is NOT implemented
// for the following reasons:
//
// 1. Constitution violation: Zero allocations after construction (Section III)
//    - Resizing requires new allocation + copy
//    - Cannot allocate in real-time path
//
// 2. Race condition complexity:
//    - Producer/consumer may be accessing buffer during resize
//    - Lock-free synchronization for resize is extremely complex
//    - Risk of data loss or corruption
//
// 3. Pragmatic solution:
//    - Buffer size determined at session initialization based on preset
//    - Preset changes requiring different buffer sizes → restart session
//    - Session restart takes <1 second, acceptable UX trade-off
//
// Implementation strategy for runtime preset changes:
//   - If new preset requires same buffer size: apply in-place (Quality ↔ Battery saver)
//   - If new preset requires different buffer size: require session restart (Low-latency ↔ others)
//
// Future enhancement (post-MVP):
//   - Double-buffering technique: allocate new queue, drain old queue, swap pointers
//   - Requires careful synchronization and testing with TSan
//   - Estimated effort: 2-3 days for implementation + validation
