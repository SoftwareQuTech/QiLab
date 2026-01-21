#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <memory>
#include <utility>

template <typename T, size_t Size>
class ThreadSafeRingBuffer
{
    static_assert((Size & (Size - 1)) == 0, "Size must be a power of 2");
    static constexpr size_t SIZE_MASK = Size - 1;

    alignas(64) std::array<T, Size> buffer;
    alignas(64) std::atomic<size_t> write_idx{0};
    alignas(64) std::atomic<size_t> read_idx{0};

public:
    bool push(T &&item) noexcept {
        size_t current, next;
        do {
            current = write_idx.load(std::memory_order_relaxed);
            next = (current + 1) & SIZE_MASK;
            
            if (next == read_idx.load(std::memory_order_acquire)) {
                return false;  // Buffer full
            }
            
            buffer[current] = std::move(item);  // Write inside the loop
            
            if (write_idx.compare_exchange_weak(current, next,
                                              std::memory_order_release,
                                              std::memory_order_relaxed)) {
                return true;  // Success
            }
            
            // If CAS failed, move item back since we'll try again
            item = std::move(buffer[current]);
        } while (true);
    }

    bool pop(T& item) noexcept {
        size_t current, next;
        do {
            current = read_idx.load(std::memory_order_relaxed);
            if (current == write_idx.load(std::memory_order_acquire)) {
                return false;  // Buffer empty
            }
            
            next = (current + 1) & SIZE_MASK;
            item = std::move(buffer[current]);  // Read inside the loop
            
            if (read_idx.compare_exchange_weak(current, next,
                                             std::memory_order_release,
                                             std::memory_order_relaxed)) {
                return true;  // Success
            }
        } while (true);
    }
};