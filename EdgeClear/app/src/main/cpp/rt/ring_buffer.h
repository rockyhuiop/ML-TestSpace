#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace edgeclear {
namespace rt {

/**
 * SPSCRingBuffer: Single-Producer Single-Consumer lock-free ring buffer
 *
 * Thread-safe queue for real-time audio communication between:
 * - Producer: RT audio callback (AAudio thread)
 * - Consumer: DSP worker thread
 *
 * Constitution requirements:
 * - Zero allocations after construction
 * - Lock-free (no mutexes in RT path)
 * - Memory fences: acquire/release ordering for ARM weak memory model
 *
 * Memory ordering rationale (from research.md R3):
 * - memory_order_acquire (consumer reads index): Ensures data visible before reading stale values
 * - memory_order_release (producer writes index): Ensures data written before index update
 * - memory_order_relaxed (same-thread reads): No synchronization needed
 */
template <typename T>
class SPSCRingBuffer {
public:
    explicit SPSCRingBuffer(size_t capacity)
        : capacity_(capacity + 1),  // +1 for full/empty distinction
          buffer_(new T[capacity + 1]),
          write_idx_(0),
          read_idx_(0) {
    }

    ~SPSCRingBuffer() {
        delete[] buffer_;
    }

    // Non-copyable, non-movable (prevent RT hazards)
    SPSCRingBuffer(const SPSCRingBuffer&) = delete;
    SPSCRingBuffer& operator=(const SPSCRingBuffer&) = delete;

    /**
     * Push element to queue (called by producer thread).
     *
     * Returns false if queue full (overflow), true on success.
     * On overflow, caller should increment XRun counter.
     */
    bool Push(const T& item) {
        const size_t current_write = write_idx_.load(std::memory_order_relaxed);
        const size_t next_write = (current_write + 1) % capacity_;

        // Check if queue full
        // Use acquire fence to ensure we see latest read_idx_ update from consumer
        if (next_write == read_idx_.load(std::memory_order_acquire)) {
            return false;  // Queue full, drop frame
        }

        // Write data
        buffer_[current_write] = item;

        // Publish write with release fence
        // Ensures buffer_[current_write] write completes before index update
        write_idx_.store(next_write, std::memory_order_release);

        return true;
    }

    /**
     * Pop element from queue (called by consumer thread).
     *
     * Returns false if queue empty, true on success.
     * Output written to 'item' reference.
     */
    bool Pop(T& item) {
        const size_t current_read = read_idx_.load(std::memory_order_relaxed);

        // Check if queue empty
        // Use acquire fence to ensure we see latest write_idx_ update from producer
        if (current_read == write_idx_.load(std::memory_order_acquire)) {
            return false;  // Queue empty
        }

        // Read data (acquire fence above ensures data is visible)
        item = buffer_[current_read];

        // Update read index with release fence
        // Not strictly required for SPSC (consumer doesn't need to sync with itself),
        // but included for consistency and future MPSC extension
        const size_t next_read = (current_read + 1) % capacity_;
        read_idx_.store(next_read, std::memory_order_release);

        return true;
    }

    /**
     * Check available space (producer perspective).
     * May be stale due to concurrent consumer, but safe for heuristics.
     */
    size_t GetAvailableSpace() const {
        const size_t current_write = write_idx_.load(std::memory_order_relaxed);
        const size_t current_read = read_idx_.load(std::memory_order_acquire);

        if (current_write >= current_read) {
            return capacity_ - 1 - (current_write - current_read);
        } else {
            return current_read - current_write - 1;
        }
    }

    /**
     * Check available items (consumer perspective).
     * May be stale due to concurrent producer, but safe for heuristics.
     */
    size_t GetAvailableItems() const {
        const size_t current_write = write_idx_.load(std::memory_order_acquire);
        const size_t current_read = read_idx_.load(std::memory_order_relaxed);

        if (current_write >= current_read) {
            return current_write - current_read;
        } else {
            return capacity_ - (current_read - current_write);
        }
    }

    size_t GetCapacity() const { return capacity_ - 1; }

private:
    const size_t capacity_;  // Actual capacity + 1
    T* buffer_;

    // Atomic indices with explicit memory ordering
    // Aligned to cache line (64 bytes) to prevent false sharing
    alignas(64) std::atomic<size_t> write_idx_;
    alignas(64) std::atomic<size_t> read_idx_;
};

}  // namespace rt
}  // namespace edgeclear
