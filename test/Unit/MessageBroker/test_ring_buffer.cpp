#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <mutex>

#include "MessageBroker/thread_safe_ring_buffer.hpp"

namespace {

/// Upper bound for the spin loops below. These tests wait for items to travel
/// between threads; if the queue ever loses one, the loop must give up so the
/// test fails on its assertions instead of spinning forever and hanging CI.
constexpr std::chrono::seconds SPIN_TIMEOUT{30};

/// Per-thread spin budget. Generous enough never to trip on a healthy queue.
struct SpinDeadline {
    std::chrono::steady_clock::time_point start{std::chrono::steady_clock::now()};
    bool expired() const noexcept {
        return std::chrono::steady_clock::now() - start > SPIN_TIMEOUT;
    }
};

} // namespace

// Test: Basic push and pop
TEST(ThreadSafeRingBufferBasicTest, PushAndPop) {
    ThreadSafeRingBuffer<int, 8> buffer;
    
    int val1 = 1, val2 = 2, val3 = 3;
    EXPECT_TRUE(buffer.push(std::move(val1)));
    EXPECT_TRUE(buffer.push(std::move(val2)));
    EXPECT_TRUE(buffer.push(std::move(val3)));
    
    int value;
    EXPECT_TRUE(buffer.pop(value));
    EXPECT_EQ(value, 1);
    
    EXPECT_TRUE(buffer.pop(value));
    EXPECT_EQ(value, 2);
    
    EXPECT_TRUE(buffer.pop(value));
    EXPECT_EQ(value, 3);
}

// Test: Empty buffer behavior
TEST(ThreadSafeRingBufferBasicTest, PopFromEmpty) {
    ThreadSafeRingBuffer<int, 4> buffer;
    
    int value;
    EXPECT_FALSE(buffer.pop(value));  // Should return false
}

// Test: Full buffer behavior
TEST(ThreadSafeRingBufferBasicTest, PushToFull) {
    ThreadSafeRingBuffer<int, 4> buffer;

    // Per-slot sequence numbers make all Size slots usable (no sacrificed slot).
    int v1 = 1, v2 = 2, v3 = 3, v4 = 4;
    EXPECT_TRUE(buffer.push(std::move(v1)));
    EXPECT_TRUE(buffer.push(std::move(v2)));
    EXPECT_TRUE(buffer.push(std::move(v3)));
    EXPECT_TRUE(buffer.push(std::move(v4)));

    // Buffer should be full now
    int v5 = 5;
    EXPECT_FALSE(buffer.push(std::move(v5)));

    // Pop one item
    int value;
    EXPECT_TRUE(buffer.pop(value));
    EXPECT_EQ(value, 1);

    // Now we can push again
    int v6 = 6;
    EXPECT_TRUE(buffer.push(std::move(v6)));
}

// Test: A rejected push must leave the item with the caller, not consume it.
// The caller owns the payload (a unique_ptr in production) and needs it back to
// retry or to drop it deliberately.
TEST(ThreadSafeRingBufferBasicTest, RejectedPushDoesNotStealItem) {
    ThreadSafeRingBuffer<std::unique_ptr<int>, 2> buffer;

    EXPECT_TRUE(buffer.push(std::make_unique<int>(1)));
    EXPECT_TRUE(buffer.push(std::make_unique<int>(2)));

    auto rejected = std::make_unique<int>(3);
    EXPECT_FALSE(buffer.push(std::move(rejected)));
    ASSERT_NE(rejected.get(), nullptr) << "full push must not take ownership";
    EXPECT_EQ(*rejected, 3);
}

// Test: FIFO order guarantee
TEST(ThreadSafeRingBufferBasicTest, FIFOOrder) {
    ThreadSafeRingBuffer<int, 16> buffer;
    
    // Push sequence
    for (int i = 0; i < 10; ++i) {
        int val = i;
        EXPECT_TRUE(buffer.push(std::move(val)));
    }
    
    // Pop and verify order
    for (int i = 0; i < 10; ++i) {
        int value;
        EXPECT_TRUE(buffer.pop(value));
        EXPECT_EQ(value, i);
    }
}

// Test: Wrap-around behavior
TEST(ThreadSafeRingBufferBasicTest, WrapAround) {
    ThreadSafeRingBuffer<int, 4> buffer;
    
    // Fill buffer
    int v1 = 1, v2 = 2, v3 = 3;
    buffer.push(std::move(v1));
    buffer.push(std::move(v2));
    buffer.push(std::move(v3));
    
    // Pop some
    int value;
    buffer.pop(value);
    buffer.pop(value);
    
    // Push more (should wrap around)
    int v4 = 4, v5 = 5;
    EXPECT_TRUE(buffer.push(std::move(v4)));
    EXPECT_TRUE(buffer.push(std::move(v5)));
    
    // Verify order
    buffer.pop(value);
    EXPECT_EQ(value, 3);
    buffer.pop(value);
    EXPECT_EQ(value, 4);
    buffer.pop(value);
    EXPECT_EQ(value, 5);
}

// Test: Single producer, single consumer
TEST(ThreadSafeRingBufferThreadTest, SingleProducerSingleConsumer) {
    ThreadSafeRingBuffer<int, 128> buffer;
    const int NUM_ITEMS = 10000;
    std::atomic<int> consumed_count{0};
    std::vector<int> consumed_values;
    consumed_values.reserve(NUM_ITEMS);
    std::mutex values_mutex;
    
    // Producer thread
    std::thread producer([&buffer, NUM_ITEMS]() {
        SpinDeadline deadline;
        for (int i = 0; i < NUM_ITEMS; ++i) {
            int val = i;
            while (!buffer.push(std::move(val)) && !deadline.expired()) {
                val = i;  // Restore value if push failed
                std::this_thread::yield();
            }
        }
    });
    
    // Consumer thread
    std::thread consumer([&buffer, &consumed_count, &consumed_values, &values_mutex, NUM_ITEMS]() {
        SpinDeadline deadline;
        int value;
        while (consumed_count < NUM_ITEMS && !deadline.expired()) {
            if (buffer.pop(value)) {
                std::lock_guard<std::mutex> lock(values_mutex);
                consumed_values.push_back(value);
                consumed_count++;
            } else {
                std::this_thread::yield();
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    // Verify all items consumed
    EXPECT_EQ(consumed_count, NUM_ITEMS);
    
    // Verify FIFO order
    for (int i = 0; i < NUM_ITEMS; ++i) {
        EXPECT_EQ(consumed_values[i], i);
    }
}

// Test: Multiple producers, single consumer
TEST(ThreadSafeRingBufferThreadTest, MultipleProducersSingleConsumer) {
    ThreadSafeRingBuffer<int, 128> buffer;
    const int NUM_PRODUCERS = 4;
    const int ITEMS_PER_PRODUCER = 1000;
    const int TOTAL_ITEMS = NUM_PRODUCERS * ITEMS_PER_PRODUCER;
    
    std::atomic<int> consumed_count{0};
    std::vector<int> consumed_values;
    std::mutex consumed_mutex;
    
    // Producer threads
    std::vector<std::thread> producers;
    for (int p = 0; p < NUM_PRODUCERS; ++p) {
        producers.emplace_back([&buffer, p, ITEMS_PER_PRODUCER]() {
            SpinDeadline deadline;
            int base = p * ITEMS_PER_PRODUCER;
            for (int i = 0; i < ITEMS_PER_PRODUCER; ++i) {
                int value = base + i;
                while (!buffer.push(std::move(value)) && !deadline.expired()) {
                    value = base + i;  // Restore if failed
                    std::this_thread::yield();
                }
            }
        });
    }
    
    // Consumer thread
    std::thread consumer([&]() {
        SpinDeadline deadline;
        int value;
        while (consumed_count < TOTAL_ITEMS && !deadline.expired()) {
            if (buffer.pop(value)) {
                std::lock_guard<std::mutex> lock(consumed_mutex);
                consumed_values.push_back(value);
                consumed_count++;
            } else {
                std::this_thread::yield();
            }
        }
    });
    
    for (auto& t : producers) {
        t.join();
    }
    consumer.join();
    
    // Verify all items consumed
    EXPECT_EQ(consumed_count, TOTAL_ITEMS);
    EXPECT_EQ(consumed_values.size(), TOTAL_ITEMS);
    

}

// Test: Concurrent producers must neither lose, duplicate nor corrupt payloads.
//
// Regression test: an earlier implementation wrote the payload into the slot
// *before* claiming it with the CAS, so two producers racing on the same slot
// destroyed one message, published the other producer's message, and then the
// CAS loser moved that same message back out of the already-published slot.
// Symptoms were empty (null) payloads reaching consumers, leaked/dropped
// commands, and producer/consumer races on the same unique_ptr. Counting pushes
// against pops cannot see any of that -- the payloads have to be verified.
TEST(ThreadSafeRingBufferThreadTest, ConcurrentProducersPreservePayloads) {
    // A move-only payload that also carries its own identity, so a stolen or
    // overwritten item is detectable rather than silently plausible.
    struct Message {
        std::unique_ptr<int> payload;
        int id{-1};
    };

    const int NUM_PRODUCERS = 4;
    const int ITEMS_PER_PRODUCER = 60;
    const int TOTAL_ITEMS = NUM_PRODUCERS * ITEMS_PER_PRODUCER; // fits in 256

    ThreadSafeRingBuffer<Message, 256> buffer;
    std::atomic<int> pushed{0};

    std::vector<std::thread> producers;
    for (int p = 0; p < NUM_PRODUCERS; ++p) {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < ITEMS_PER_PRODUCER; ++i) {
                Message msg;
                msg.id = p * ITEMS_PER_PRODUCER + i;
                msg.payload = std::make_unique<int>(msg.id);
                if (buffer.push(std::move(msg))) pushed++;
            }
        });
    }
    for (auto& t : producers) {
        t.join();
    }

    // Everything fits in the buffer, so nothing may be rejected.
    ASSERT_EQ(pushed, TOTAL_ITEMS);

    std::vector<int> times_seen(TOTAL_ITEMS, 0);
    int popped = 0;
    int empty_payloads = 0;
    int mismatched_payloads = 0;

    Message out;
    while (buffer.pop(out)) {
        ++popped;
        if (!out.payload) {
            ++empty_payloads;
        } else {
            if (*out.payload != out.id) ++mismatched_payloads;
            if (out.id >= 0 && out.id < TOTAL_ITEMS) times_seen[out.id]++;
        }
        out.payload.reset();
    }

    EXPECT_EQ(popped, TOTAL_ITEMS);
    EXPECT_EQ(empty_payloads, 0) << "a slot was published without its payload";
    EXPECT_EQ(mismatched_payloads, 0) << "a slot was published with another item's payload";

    int missing = 0, duplicated = 0;
    for (int id = 0; id < TOTAL_ITEMS; ++id) {
        if (times_seen[id] == 0) ++missing;
        else if (times_seen[id] > 1) ++duplicated;
    }
    EXPECT_EQ(missing, 0) << "producer lost messages";
    EXPECT_EQ(duplicated, 0) << "a message was delivered more than once";
}

// Test: Single producer, multiple consumers
TEST(ThreadSafeRingBufferThreadTest, SingleProducerMultipleConsumers) {
    ThreadSafeRingBuffer<int, 128> buffer;
    const int NUM_CONSUMERS = 4;
    const int TOTAL_ITEMS = 10000;
    
    std::atomic<int> consumed_count{0};
    std::vector<std::atomic<int>> per_consumer_count(NUM_CONSUMERS);
    
    // Producer thread
    std::thread producer([&]() {
        SpinDeadline deadline;
        for (int i = 0; i < TOTAL_ITEMS; ++i) {
            int val = i;
            while (!buffer.push(std::move(val)) && !deadline.expired()) {
                val = i;
                std::this_thread::yield();
            }
        }
    });
    
    // Consumer threads
    std::vector<std::thread> consumers;
    for (int c = 0; c < NUM_CONSUMERS; ++c) {
        consumers.emplace_back([&, c]() {
            SpinDeadline deadline;
            int value;
            while (consumed_count < TOTAL_ITEMS && !deadline.expired()) {
                if (buffer.pop(value)) {
                    per_consumer_count[c]++;
                    consumed_count++;
                    std::this_thread::yield();
                }
            }
        });
    }
    
    producer.join();
    for (auto& t : consumers) {
        t.join();
    }
    
    // Verify all items consumed
    EXPECT_EQ(consumed_count, TOTAL_ITEMS);
    
    // Verify work was distributed
    int total_distributed = 0;
    for (int c = 0; c < NUM_CONSUMERS; ++c) {
        EXPECT_GT(per_consumer_count[c], 0);
        total_distributed += per_consumer_count[c];
    }
    EXPECT_EQ(total_distributed, TOTAL_ITEMS);
}

// Test: Stress test with rapid push/pop
TEST(ThreadSafeRingBufferStressTest, RapidOperations) {
    ThreadSafeRingBuffer<int, 64> buffer;
    const int NUM_ITERATIONS = 100000;
    std::atomic<int> push_count{0};
    std::atomic<int> pop_count{0};
    
    // Rapid pusher
    std::thread pusher([&]() {
        SpinDeadline deadline;
        for (int i = 0; i < NUM_ITERATIONS; ++i) {
            int val = i;
            while (!buffer.push(std::move(val)) && !deadline.expired()) {
                val = i;
                std::this_thread::yield();
            }
            push_count++;
        }
    });
    
    // Rapid popper
    std::thread popper([&]() {
        SpinDeadline deadline;
        int value;
        while (pop_count < NUM_ITERATIONS && !deadline.expired()) {
            if (buffer.pop(value)) {
                pop_count++;
            } else {
                std::this_thread::yield();
            }
        }
    });
    
    pusher.join();
    popper.join();
    
    EXPECT_EQ(push_count, NUM_ITERATIONS);
    EXPECT_EQ(pop_count, NUM_ITERATIONS);
}

// Test: Move-only types (unique_ptr)
TEST(ThreadSafeRingBufferMoveTest, UniquePointer) {
    ThreadSafeRingBuffer<std::unique_ptr<int>, 8> buffer;
    
    auto ptr1 = std::make_unique<int>(42);
    auto ptr2 = std::make_unique<int>(84);
    
    EXPECT_TRUE(buffer.push(std::move(ptr1)));
    EXPECT_TRUE(buffer.push(std::move(ptr2)));
    
    std::unique_ptr<int> result;
    EXPECT_TRUE(buffer.pop(result));
    EXPECT_EQ(*result, 42);
    
    EXPECT_TRUE(buffer.pop(result));
    EXPECT_EQ(*result, 84);
}

// Test: Large objects
TEST(ThreadSafeRingBufferLargeObjectTest, LargeStructs) {
    struct LargeObject {
        std::array<int, 256> data;
        LargeObject(int val = 0) { data.fill(val); }
        LargeObject(const LargeObject&) = default;
        LargeObject(LargeObject&&) = default;
        LargeObject& operator=(const LargeObject&) = default;
        LargeObject& operator=(LargeObject&&) = default;
    };
    
    ThreadSafeRingBuffer<LargeObject, 8> buffer;
    
    LargeObject obj1(1);
    LargeObject obj2(2);
    buffer.push(std::move(obj1));
    buffer.push(std::move(obj2));
    
    LargeObject obj;
    EXPECT_TRUE(buffer.pop(obj));
    EXPECT_EQ(obj.data[0], 1);
}

// Test: Alternating push/pop pattern
TEST(ThreadSafeRingBufferPatternTest, AlternatingOperations) {
    ThreadSafeRingBuffer<int, 8> buffer;
    
    for (int i = 0; i < 100; ++i) {
        int val = i;
        EXPECT_TRUE(buffer.push(std::move(val)));
        
        int value;
        EXPECT_TRUE(buffer.pop(value));
        EXPECT_EQ(value, i);
    }
}

// Test: Multiple threads pushing and popping simultaneously
TEST(ThreadSafeRingBufferThreadTest, HighContention) {
    ThreadSafeRingBuffer<int, 256> buffer;
    const int NUM_THREADS = 8;
    const int OPERATIONS_PER_THREAD = 10000;
    
    std::atomic<int> total_pushed{0};
    std::atomic<int> total_popped{0};
    
    std::vector<std::thread> threads;
    
    // Half push, half pop
    for (int i = 0; i < NUM_THREADS / 2; ++i) {
        threads.emplace_back([&, i]() {
            SpinDeadline deadline;
            for (int j = 0; j < OPERATIONS_PER_THREAD; ++j) {
                int value = i * OPERATIONS_PER_THREAD + j;
                while (!buffer.push(std::move(value)) && !deadline.expired()) {
                    value = i * OPERATIONS_PER_THREAD + j;
                    std::this_thread::yield();
                }
                total_pushed++;
            }
        });
    }
    
    for (int i = 0; i < NUM_THREADS / 2; ++i) {
        threads.emplace_back([&]() {
            SpinDeadline deadline;
            int value;
            int popped = 0;
            while (popped < OPERATIONS_PER_THREAD && !deadline.expired()) {
                if (buffer.pop(value)) {
                    popped++;
                    total_popped++;
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    EXPECT_EQ(total_pushed, (NUM_THREADS / 2) * OPERATIONS_PER_THREAD);
    EXPECT_EQ(total_popped, (NUM_THREADS / 2) * OPERATIONS_PER_THREAD);
}

// Test: Performance benchmark
TEST(ThreadSafeRingBufferPerformanceTest, Throughput) {
    ThreadSafeRingBuffer<int, 1024> buffer;
    const int NUM_OPERATIONS = 1000000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    std::thread producer([&]() {
        SpinDeadline deadline;
        for (int i = 0; i < NUM_OPERATIONS; ++i) {
            int val = i;
            while (!buffer.push(std::move(val)) && !deadline.expired()) {
                val = i;
                std::this_thread::yield();
            }
        }
    });
    
    std::thread consumer([&]() {
        SpinDeadline deadline;
        int value;
        for (int i = 0; i < NUM_OPERATIONS; ++i) {
            while (!buffer.pop(value) && !deadline.expired()) {
                std::this_thread::yield();
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    double ops_per_sec = (NUM_OPERATIONS * 2.0) / (duration.count() / 1000.0);
    std::cout << "Throughput: " << ops_per_sec << " ops/sec" << std::endl;
    
    // Assert minimum performance
    EXPECT_GT(ops_per_sec, 100000);
}

// Test: Power of 2 size enforcement (compile-time)
TEST(ThreadSafeRingBufferCompileTest, PowerOfTwoSize) {
    // These should compile fine
    ThreadSafeRingBuffer<int, 2> buf2;
    ThreadSafeRingBuffer<int, 4> buf4;
    ThreadSafeRingBuffer<int, 8> buf8;
    ThreadSafeRingBuffer<int, 16> buf16;
    ThreadSafeRingBuffer<int, 1024> buf1024;
}