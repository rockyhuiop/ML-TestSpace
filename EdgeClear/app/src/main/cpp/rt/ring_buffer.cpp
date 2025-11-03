// ring_buffer.cpp - Implementation file
// SPSCRingBuffer is template class (header-only)
#include "ring_buffer.h"

// Explicit template instantiations for common types
namespace edgeclear {
namespace rt {

// Forward declare DSPFrameBuffer to avoid circular dependency
struct DSPFrameBuffer;

// Instantiate for DSPFrameBuffer (most common use case)
template class SPSCRingBuffer<DSPFrameBuffer>;

}  // namespace rt
}  // namespace edgeclear
