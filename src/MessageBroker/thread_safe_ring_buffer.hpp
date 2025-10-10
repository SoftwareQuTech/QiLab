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
    bool pop(T &item) noexcept
    {
        size_t current, next;
        do
        {
            current = read_idx.load(std::memory_order_relaxed);
            if (current == write_idx.load(std::memory_order_acquire))
            {
                return false; // Buffer empty
            }

            next = (current + 1) & SIZE_MASK;
        } while (!read_idx.compare_exchange_weak(current, next,
                                                 std::memory_order_release,
                                                 std::memory_order_relaxed));

        item = std::move(buffer[current]);
        return true;
    }
    bool push(T &&item)
    {
        size_t current, next;
        do
        {
            current = write_idx.load(std::memory_order_relaxed);
            next = (current + 1) & SIZE_MASK;

            if (next == read_idx.load(std::memory_order_acquire))
            {
                return false; // Buffer full
            }
        } while (!write_idx.compare_exchange_weak(current, next,
                                                  std::memory_order_release,
                                                  std::memory_order_relaxed));

        buffer[current] = std::move(item);
        return true;
    }
};