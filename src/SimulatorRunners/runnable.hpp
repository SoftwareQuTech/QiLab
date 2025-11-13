/*
** Created on: 08.09.2025
** Created by: mrkolev
** Description: Conatains the IRunnable class
*/

#pragma once
#include <cstdint>
#include <atomic>
#include <array>
#include <memory>

template <typename T, size_t Size>
class LockFreeRingBuffer
{
    std::array<T, Size> buffer;
    std::atomic<size_t> write_idx{0};
    std::atomic<size_t> read_idx{0};

public:
    bool push(T &&item)
    {
        auto current = write_idx.load(std::memory_order_relaxed);
        auto next = (current + 1) % Size;
        if (next == read_idx.load(std::memory_order_acquire))
            return false; // Buffer full

        buffer[current] = std::move(item);
        write_idx.store(next, std::memory_order_release);
        return true;
    }

    bool pop(T &item)
    {
        auto current = read_idx.load(std::memory_order_relaxed);
        if (current == write_idx.load(std::memory_order_acquire))
            return false; // Buffer empty

        item = std::move(buffer[current]);
        read_idx.store((current + 1) % Size, std::memory_order_release);
        return true;
    }
};

template <typename T>
struct IRunnable
{
    LockFreeRingBuffer<T, 128> lockfree_queue; // Example size

    void notify(T value)
    {
        lockfree_queue.push(std::move(value));
    }
    virtual int on_run() = 0;
    virtual void on_abort() = 0;
};