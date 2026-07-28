/**
 * @file thread_safe_ring_buffer.hpp
 * @brief Lock-free bounded multi-producer / multi-consumer ring buffer.
 */

#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

/**
 * @brief Lock-free bounded MPMC queue (Vyukov-style, per-slot sequence numbers).
 *
 * Each slot carries a monotonically increasing sequence counter that encodes
 * whose turn it is to touch the slot. A thread must *claim* a slot (CAS on the
 * shared ticket counter) before it reads or writes the payload, and only then
 * publishes its work by advancing the slot sequence with a release store:
 *
 *   - slot free for the producer at ticket `pos`  <=> sequence == pos
 *   - slot holds an item for the consumer at `pos` <=> sequence == pos + 1
 *   - anything else means the queue is full (push) or empty (pop)
 *
 * Because the payload is only touched by the thread that owns the ticket, no
 * two threads can ever write the same slot, and a consumer can never observe a
 * half-published item. This is what makes it safe for move-only payloads such
 * as `TransportData<CommandBase>` (which owns its command via `unique_ptr`).
 *
 * `write_idx`/`read_idx` are free-running tickets (never masked); only the slot
 * lookup is masked, so wrap-around needs no separate "one slot always empty"
 * sacrifice: the usable capacity is the full @p Size.
 *
 * @tparam T    Payload type; must be default-constructible and nothrow
 *              move-assignable (push/pop are noexcept and move the payload).
 * @tparam Size Number of slots, must be a power of two.
 *
 * @note push() and pop() are lock-free (not wait-free): a claim may need to be
 *       retried while other threads claim concurrently, but no thread can block
 *       another indefinitely and no operation ever sleeps or allocates.
 */
template <typename T, size_t Size>
class ThreadSafeRingBuffer
{
    static_assert((Size & (Size - 1)) == 0, "Size must be a power of 2");
    static_assert(std::is_nothrow_move_assignable_v<T>,
                  "T must be nothrow move-assignable: push()/pop() are noexcept");
    static_assert(std::is_default_constructible_v<T>,
                  "T must be default-constructible to pre-allocate the slots");

    static constexpr size_t SIZE_MASK = Size - 1;

    /// One queue slot: the payload plus the sequence number that owns it.
    struct Slot {
        std::atomic<size_t> sequence;
        T value;
    };

    alignas(64) std::array<Slot, Size> buffer{};
    alignas(64) std::atomic<size_t> write_idx{0}; ///< Next producer ticket
    alignas(64) std::atomic<size_t> read_idx{0};  ///< Next consumer ticket

public:
    ThreadSafeRingBuffer() noexcept {
        // Slot i starts out free for the producer holding ticket i.
        for (size_t i = 0; i < Size; ++i) {
            buffer[i].sequence.store(i, std::memory_order_relaxed);
        }
    }

    ThreadSafeRingBuffer(const ThreadSafeRingBuffer&) = delete;
    ThreadSafeRingBuffer& operator=(const ThreadSafeRingBuffer&) = delete;

    /**
     * @brief Enqueues an item, taking ownership on success.
     *
     * @param item Item to move into the queue. Left untouched if the queue is
     *             full, so the caller keeps ownership and may retry or drop it.
     * @return true if the item was enqueued, false if the queue is full.
     */
    bool push(T &&item) noexcept {
        Slot* slot;
        size_t pos = write_idx.load(std::memory_order_relaxed);

        for (;;) {
            slot = &buffer[pos & SIZE_MASK];
            const size_t seq = slot->sequence.load(std::memory_order_acquire);
            const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);

            if (diff == 0) {
                // Slot is free and it is our turn: try to claim ticket `pos`.
                if (write_idx.compare_exchange_weak(pos, pos + 1,
                                                    std::memory_order_relaxed,
                                                    std::memory_order_relaxed)) {
                    break;
                }
                // Lost the race; `pos` now holds the current ticket, retry.
            } else if (diff < 0) {
                return false; // Producer lapped the consumer: queue is full.
            } else {
                // Another producer already took this ticket, re-read and retry.
                pos = write_idx.load(std::memory_order_relaxed);
            }
        }

        // The ticket is ours, so the slot is ours exclusively: safe to write.
        slot->value = std::move(item);
        slot->sequence.store(pos + 1, std::memory_order_release); // Publish
        return true;
    }

    /**
     * @brief Dequeues the oldest item.
     *
     * @param item Output parameter; assigned the dequeued item on success and
     *             left untouched when the queue is empty.
     * @return true if an item was dequeued, false if the queue is empty.
     */
    bool pop(T& item) noexcept {
        Slot* slot;
        size_t pos = read_idx.load(std::memory_order_relaxed);

        for (;;) {
            slot = &buffer[pos & SIZE_MASK];
            const size_t seq = slot->sequence.load(std::memory_order_acquire);
            const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);

            if (diff == 0) {
                // Slot holds a published item and it is our turn: claim it.
                if (read_idx.compare_exchange_weak(pos, pos + 1,
                                                   std::memory_order_relaxed,
                                                   std::memory_order_relaxed)) {
                    break;
                }
                // Lost the race; `pos` now holds the current ticket, retry.
            } else if (diff < 0) {
                return false; // Nothing published at `pos`: queue is empty.
            } else {
                // Another consumer already took this ticket, re-read and retry.
                pos = read_idx.load(std::memory_order_relaxed);
            }
        }

        // The ticket is ours, so the slot is ours exclusively: safe to read.
        item = std::move(slot->value);
        // Hand the slot to the producer one lap ahead (ticket pos + Size).
        slot->sequence.store(pos + SIZE_MASK + 1, std::memory_order_release);
        return true;
    }
};
