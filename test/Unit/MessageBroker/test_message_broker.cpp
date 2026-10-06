// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <memory>

#include "MessageBroker/message_broker.hpp"
#include "DataTypes/commands.hpp"
#include "DataTypes/responses.hpp"

namespace {

/// Upper bound for the consumer spin loops below. If the broker ever loses a
/// message, the loop must give up so the test fails on its assertions instead
/// of spinning forever and hanging the CI job.
constexpr std::chrono::seconds SPIN_TIMEOUT{30};

/// Per-thread spin budget. Generous enough never to trip on a healthy broker.
struct SpinDeadline {
    std::chrono::steady_clock::time_point start{std::chrono::steady_clock::now()};
    bool expired() const noexcept {
        return std::chrono::steady_clock::now() - start > SPIN_TIMEOUT;
    }
};

} // namespace

// Test fixture for MessageBroker tests
class MessageBrokerTest : public ::testing::Test {
protected:
    std::shared_ptr<MessageBroker> broker;

    void SetUp() override {
        broker = std::make_shared<MessageBroker>();
    }

    // Helper to create test commands wrapped in TransportData
    TransportData<CommandBase> make_transport_command(Command_t type, uint16_t node_id, DestinationType dest, DestinationType src = DestinationType::CommandRunner) {
        TransportData<CommandBase> data;
        uint8_t buffer[4] = {0};
        data.destination = dest;
        data.source = src;
        data.node_id = node_id;
        data.transport = std::make_unique<CommandBase>(buffer);
        data.transport->command_type = type;
        return data;
    }

    TransportData<CommandBase> make_entanglement_transport(uint16_t node_id, uint16_t neighbour) {
        TransportData<CommandBase> data;
        uint8_t buffer[4] = {0};
        data.node_id = node_id;
        data.destination = DestinationType::EntanglementRunner;
        data.source = DestinationType::CommandRunner;
        auto cmd = std::make_unique<EntanglementAttempt>(buffer);
        cmd->command_type = Command_t::ENTANGLEMENT_ATTEMPT;
        cmd->nghbr = neighbour;
        data.transport = std::move(cmd);
        return data;
    }

    TransportData<ResponseData> make_transport_response(uint16_t node_id, Outcome_Status_t status, DestinationType src, DestinationType dest = DestinationType::CommandRunner) {
        TransportData<ResponseData> data;
        data.node_id = node_id;
        data.destination = dest;
        data.source = src;
        data.transport = std::make_unique<ResponseData>();
        data.transport->outcome = status;
        return data;
    }
};

// Test: Basic command posting and retrieval
TEST_F(MessageBrokerTest, PostAndGetCommand) {
    auto cmd_data = make_transport_command(Command_t::ENTANGLEMENT_ATTEMPT, 1, DestinationType::SimRunner);
    
    // Post command
    EXPECT_TRUE(broker->route_command(std::move(cmd_data)));
    
    // Retrieve command
    TransportData<CommandBase> retrieved;
    EXPECT_TRUE(broker->get_command(retrieved, DestinationType::SimRunner));
    EXPECT_EQ(retrieved.node_id, 1);
    EXPECT_EQ(retrieved.transport->command_type, Command_t::ENTANGLEMENT_ATTEMPT);
}

// Test: Get from empty queue
TEST_F(MessageBrokerTest, GetCommandFromEmpty) {
    TransportData<CommandBase> retrieved;
    EXPECT_FALSE(broker->get_command(retrieved, DestinationType::SimRunner));
}

// Test: Multiple destinations
TEST_F(MessageBrokerTest, MultipleDifferentDestinations) {
    auto cmd1 = make_transport_command(Command_t::ENTANGLEMENT_ATTEMPT, 1, DestinationType::SimRunner);
    auto cmd2 = make_transport_command(Command_t::ENTANGLEMENT_MEASURE, 2, DestinationType::EntanglementRunner);
    auto cmd3 = make_transport_command(Command_t::QUBIT_MEASUREMENT, 3, DestinationType::SimRunner);
    
    // Post to different destinations
    EXPECT_TRUE(broker->route_command(std::move(cmd1)));
    EXPECT_TRUE(broker->route_command(std::move(cmd2)));
    EXPECT_TRUE(broker->route_command(std::move(cmd3)));
    
    // Retrieve from correct destinations
    TransportData<CommandBase> retrieved;
    
    EXPECT_TRUE(broker->get_command(retrieved, DestinationType::SimRunner));
    EXPECT_EQ(retrieved.node_id, 1);
    
    EXPECT_TRUE(broker->get_command(retrieved, DestinationType::EntanglementRunner));
    EXPECT_EQ(retrieved.node_id, 2);
    
    EXPECT_TRUE(broker->get_command(retrieved, DestinationType::SimRunner));
    EXPECT_EQ(retrieved.node_id, 3);
    
    // All queues should be empty now
    EXPECT_FALSE(broker->get_command(retrieved, DestinationType::SimRunner));
    EXPECT_FALSE(broker->get_command(retrieved, DestinationType::EntanglementRunner));
    EXPECT_FALSE(broker->get_command(retrieved, DestinationType::SimRunner));
}

// Test: FIFO order within same destination
TEST_F(MessageBrokerTest, FIFOOrderSameDestination) {
    const int NUM_COMMANDS = 10;
    
    // Post multiple commands
    for (int i = 0; i < NUM_COMMANDS; ++i) {
        auto cmd = make_transport_command(Command_t::ENTANGLEMENT_ATTEMPT, i, DestinationType::SimRunner);
        EXPECT_TRUE(broker->route_command(std::move(cmd)));
    }
    
    // Retrieve and verify order
    for (int i = 0; i < NUM_COMMANDS; ++i) {
        TransportData<CommandBase> retrieved;
        EXPECT_TRUE(broker->get_command(retrieved, DestinationType::SimRunner));
        EXPECT_EQ(retrieved.node_id, i);
    }
}

// Test: Response posting and retrieval
TEST_F(MessageBrokerTest, PostAndGetResponse) {
    auto response_data = make_transport_response(5, Outcome_Status_t::SUCCESS, DestinationType::SimRunner);
    
    // Post response
    EXPECT_TRUE(broker->route_response(std::move(response_data)));
    
    // Retrieve response
    TransportData<ResponseData> retrieved;
    EXPECT_TRUE(broker->get_response(retrieved));
    EXPECT_EQ(retrieved.node_id, 5);
    EXPECT_EQ(retrieved.transport->outcome, Outcome_Status_t::SUCCESS);
}

// Test: Get response from empty queue
TEST_F(MessageBrokerTest, GetResponseFromEmpty) {
    TransportData<ResponseData> retrieved;
    EXPECT_FALSE(broker->get_response(retrieved));
}

// Test: Multiple responses to different destinations
TEST_F(MessageBrokerTest, MultipleResponseDestinations) {
    // Post responses to different destinations
    auto response = make_transport_response(1, Outcome_Status_t::SUCCESS, DestinationType::SimRunner);
    EXPECT_TRUE(broker->route_response(std::move(response)));

    response = make_transport_response(1, Outcome_Status_t::SUCCESS, DestinationType::EntanglementRunner);
    EXPECT_TRUE(broker->route_response(std::move(response)));

    response = make_transport_response(1, Outcome_Status_t::SUCCESS, DestinationType::CommandRunner);
    EXPECT_FALSE(broker->route_response(std::move(response)));

    // Verify only sim and entanglmeent can route responses
    TransportData<ResponseData> retrieved;
    EXPECT_TRUE(broker->get_response(retrieved));
    EXPECT_TRUE(broker->get_response(retrieved));
    EXPECT_FALSE(broker->get_response(retrieved));
}
// Test: Concurrent producers, single consumer (commands)
//
// Counting posts against retrievals is not enough on its own: a broker that
// races on a queue slot can publish the same number of entries while silently
// destroying one command and handing out another with an empty payload. So the
// commands themselves are verified -- every one must arrive exactly once, with
// its payload intact.
TEST_F(MessageBrokerTest, ConcurrentCommandProducers) {
    const int NUM_PRODUCERS = 4;
    // Keep total commands well within ring buffer capacity (256) to avoid intentional drops
    // and simplify ASan leak checking. Using 60 per producer = 240 total to be safe.
    const int COMMANDS_PER_PRODUCER = 60;
    const int TOTAL_COMMANDS = NUM_PRODUCERS * COMMANDS_PER_PRODUCER;

    std::atomic<int> posted{0};

    // Producer threads
    std::vector<std::thread> producers;
    for (int p = 0; p < NUM_PRODUCERS; ++p) {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < COMMANDS_PER_PRODUCER; ++i) {
                auto cmd = make_transport_command(Command_t::ENTANGLEMENT_ATTEMPT,
                                                  p * COMMANDS_PER_PRODUCER + i, DestinationType::SimRunner);
                if(broker->route_command(std::move(cmd))) posted++;
            }
        });
    }

    // Wait for all posts
    for (auto& t : producers) {
        t.join();
    }

    // All commands should fit in the buffer given the capped total
    EXPECT_EQ(posted, TOTAL_COMMANDS);

    // Consume all commands
    int consumed = 0;
    int empty_payloads = 0;
    std::vector<int> times_seen(TOTAL_COMMANDS, 0);
    TransportData<CommandBase> retrieved;
    while (broker->get_command(retrieved, DestinationType::SimRunner)) {
        consumed++;
        if (!retrieved.transport) {
            empty_payloads++;
        } else {
            EXPECT_EQ(retrieved.transport->command_type, Command_t::ENTANGLEMENT_ATTEMPT);
            if (retrieved.node_id < TOTAL_COMMANDS) times_seen[retrieved.node_id]++;
        }
        retrieved.transport.reset();  // Explicitly free the command
    }

    EXPECT_EQ(consumed, posted);
    EXPECT_EQ(empty_payloads, 0) << "broker handed out a command without its payload";

    int missing = 0, duplicated = 0;
    for (int id = 0; id < TOTAL_COMMANDS; ++id) {
        if (times_seen[id] == 0) ++missing;
        else if (times_seen[id] > 1) ++duplicated;
    }
    EXPECT_EQ(missing, 0) << "commands were lost between producers";
    EXPECT_EQ(duplicated, 0) << "a command was delivered more than once";

    // Ensure broker (and any queued commands) are destroyed before the next test
    broker.reset();
}


// Test: Concurrent producers and consumers
TEST_F(MessageBrokerTest, ConcurrentProducersAndConsumers) {
    const int NUM_PRODUCERS = 2;
    const int NUM_CONSUMERS = 1;
    const int MESSAGES_PER_PRODUCER = 32;
    const int TOTAL_MESSAGES = NUM_PRODUCERS * MESSAGES_PER_PRODUCER;
    
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};
    
    // Producer threads
    std::vector<std::thread> producers;
    for (int p = 0; p < NUM_PRODUCERS; ++p) {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < MESSAGES_PER_PRODUCER; ++i) {
                auto cmd = make_transport_command(Command_t::ENTANGLEMENT_ATTEMPT,
                                                  p * MESSAGES_PER_PRODUCER + i, DestinationType::SimRunner);
                broker->route_command(std::move(cmd));
                produced++;
            }
        });
    }
    
    // Consumer threads, actually consumers are expected to be less than producers
    std::vector<std::thread> consumers;
    for (int c = 0; c < NUM_CONSUMERS; ++c) {
        consumers.emplace_back([&]() {
            SpinDeadline deadline;
            TransportData<CommandBase> retrieved;
            while (consumed < TOTAL_MESSAGES && !deadline.expired()) {
                if (broker->get_command(retrieved, DestinationType::SimRunner)) {
                    consumed++;
                    retrieved.transport.reset();  // Explicitly free the command
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }
    
    for (auto& t : producers) {
        t.join();
    }
    for (auto& t : consumers) {
        t.join();
    }
    
    EXPECT_EQ(produced, TOTAL_MESSAGES);
    EXPECT_EQ(consumed, TOTAL_MESSAGES);

    // Ensure broker teardown to release any queued items before the next test
    broker.reset();
}

// Test: Mixed commands and responses
TEST_F(MessageBrokerTest, MixedCommandsAndResponses) {
    const int NUM_ITEMS = 100;
    
    // Post interleaved commands and responses
    for (int i = 0; i < NUM_ITEMS; ++i) {
        auto cmd = make_transport_command(Command_t::ENTANGLEMENT_ATTEMPT, i, DestinationType::SimRunner);
        EXPECT_TRUE(broker->route_command(std::move(cmd)));
        
        auto response = make_transport_response(i, Outcome_Status_t::SUCCESS, DestinationType::EntanglementRunner);
        EXPECT_TRUE(broker->route_response(std::move(response)));
    }
    
    // Retrieve all commands
    int cmd_count = 0;
    TransportData<CommandBase> cmd_data;
    while (broker->get_command(cmd_data, DestinationType::SimRunner)) {
        cmd_count++;
        cmd_data.transport.reset();
    }
    EXPECT_EQ(cmd_count, NUM_ITEMS);
    
    // Retrieve all responses
    int resp_count = 0;
    TransportData<ResponseData> resp_data;
    while (broker->get_response(resp_data)) {
        resp_count++;
        resp_data.transport.reset();
    }
    EXPECT_EQ(resp_count, NUM_ITEMS);
}

// Test: Multiple destinations concurrent access
TEST_F(MessageBrokerTest, MultipleDestinationsConcurrent) {
    const int MESSAGES_PER_DEST = 100;
    std::atomic<int> total_consumed{0};
    
    // Producer threads for each destination
    std::vector<std::thread> producers;
    producers.emplace_back([&]() {
        for (int i = 0; i < MESSAGES_PER_DEST; ++i) {
            auto cmd = make_transport_command(Command_t::ENTANGLEMENT_ATTEMPT, i, DestinationType::SimRunner);
            EXPECT_TRUE(broker->route_command(std::move(cmd)));
        }
    });
    
    producers.emplace_back([&]() {
        for (int i = 0; i < MESSAGES_PER_DEST; ++i) {
            auto cmd = make_transport_command(Command_t::ENTANGLEMENT_MEASURE, i, DestinationType::EntanglementRunner);
            EXPECT_TRUE(broker->route_command(std::move(cmd)));
        }
    });
    
    // Consumer threads for each destination
    std::vector<std::thread> consumers;
    consumers.emplace_back([&]() {
        SpinDeadline deadline;
        int consumed = 0;
        TransportData<CommandBase> retrieved;
        while (consumed < MESSAGES_PER_DEST && !deadline.expired()) {
            if (broker->get_command(retrieved, DestinationType::SimRunner)) {
                consumed++;
                total_consumed++;
                retrieved.transport.reset();
            }
        }
    });

    consumers.emplace_back([&]() {
        SpinDeadline deadline;
        int consumed = 0;
        TransportData<CommandBase> retrieved;
        while (consumed < MESSAGES_PER_DEST && !deadline.expired()) {
            if (broker->get_command(retrieved, DestinationType::EntanglementRunner)) {
                consumed++;
                total_consumed++;
                retrieved.transport.reset();
            }
        }
    });

    
    for (auto& t : producers) {
        t.join();
    }
    for (auto& t : consumers) {
        t.join();
    }
    
    EXPECT_EQ(total_consumed, MESSAGES_PER_DEST * 2);
}

// Test: Move semantics correctness
TEST_F(MessageBrokerTest, MoveSemanticsCorrectness) {
    auto cmd_data = make_entanglement_transport(1, 42);
    
    // Post command (should move)
    EXPECT_TRUE(broker->route_command(std::move(cmd_data)));
    
    // Original TransportData should have null transport after move
    EXPECT_EQ(cmd_data.transport.get(), nullptr);
    
    // Retrieve command
    TransportData<CommandBase> retrieved;
    EXPECT_TRUE(broker->get_command(retrieved, DestinationType::EntanglementRunner));
    
    // Retrieved command should be valid
    EXPECT_NE(retrieved.transport.get(), nullptr);
    EXPECT_EQ(retrieved.node_id, 1);
    
    // Cast and verify data
    auto* ent_cmd = static_cast<EntanglementAttempt*>(retrieved.transport.get());
    EXPECT_EQ(ent_cmd->nghbr, 42);
}

// Test: Queue isolation - commands don't leak between destinations
TEST_F(MessageBrokerTest, QueueIsolation) {
    auto cmd1 = make_transport_command(Command_t::ENTANGLEMENT_ATTEMPT, 1, DestinationType::EntanglementRunner);
    auto cmd2 = make_transport_command(Command_t::QUBIT_MEASUREMENT, 2, DestinationType::SimRunner);

    EXPECT_TRUE(broker->route_command(std::move(cmd1)));
    EXPECT_TRUE(broker->route_command(std::move(cmd2)));

    // Try to get from wrong destination
    TransportData<CommandBase> retrieved;
    EXPECT_FALSE(broker->get_command(retrieved, DestinationType::CommandRunner));
    
    // Get from correct destinations
    EXPECT_TRUE(broker->get_command(retrieved, DestinationType::SimRunner));
    EXPECT_EQ(retrieved.node_id, 2);
    
    EXPECT_TRUE(broker->get_command(retrieved, DestinationType::EntanglementRunner));
    EXPECT_EQ(retrieved.node_id, 1);
}
