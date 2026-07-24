#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "Simulator/EntanglementManager/entanglement_manager.hpp"
#include "Simulator/NoiseModels/EntanglementGenerationNoise/entanglement_attempt_noise.hpp"
#include "Simulator/link_key.hpp"
#include "DataTypes/commands.hpp"
#include "mocks/command_mock.hpp"

using ::testing::_;
using ::testing::Return;

// Mock callbacks for testing
class EntanglementManagerTest : public ::testing::Test {
protected:
    std::vector<std::pair<int, Outcome_Status_t>> responses;
    std::vector<std::pair<int, std::unique_ptr<CommandBase>>> commands;

    std::unique_ptr<EntanglementManager> manager;

    void SetUp() override {
        auto response_cb = [this](int node_id, Outcome_Status_t status) {
            responses.push_back({node_id, status});
        };

        auto command_cb = [this](int node_id, std::unique_ptr<CommandBase> cmd) {
            commands.push_back({node_id, std::move(cmd)});
        };

        manager = std::make_unique<EntanglementManager>(response_cb, command_cb);

        // Most tests below predate per-link attempt models and just want
        // matching to succeed quickly. Configure a permissive (guaranteed
        // 1-attempt success, ~0 delay) model for every node-id pair used
        // across the fixture's tests, so those keep exercising match/timeout
        // behavior unaffected by the fail-fast path. Tests that specifically
        // exercise fail-fast configure their own pair via configure_permissive_link
        // / configure_failing_link, or rely on this range not covering their pair.
        configure_permissive_range(0, 100);

        responses.clear();
        commands.clear();
    }

    // Registers a guaranteed-success (p_success=1.0, 1 attempt, `duration_ns`
    // delay) attempt model for a specific node pair.
    void configure_permissive_link(uint16_t node_a, uint16_t node_b, uint64_t duration_ns = 100) {
        std::unordered_map<uint16_t, std::unique_ptr<EntanglementAttemptNoiseInterface>> models;
        const uint16_t key = encode_link_key(static_cast<uint8_t>(node_a), static_cast<uint8_t>(node_b));
        models[key] = std::make_unique<EntanglementAttemptModel>(duration_ns, /*batch_size=*/1, /*p_success=*/1.0f, /*rng_seed=*/1);
        manager->configure_links(std::move(models));
    }

    // Registers a guaranteed-failure (p_success=0.0) attempt model for a
    // specific node pair.
    void configure_failing_link(uint16_t node_a, uint16_t node_b) {
        std::unordered_map<uint16_t, std::unique_ptr<EntanglementAttemptNoiseInterface>> models;
        const uint16_t key = encode_link_key(static_cast<uint8_t>(node_a), static_cast<uint8_t>(node_b));
        models[key] = std::make_unique<EntanglementAttemptModel>(/*attempt_duration_ns=*/100, /*batch_size=*/1, /*p_success=*/0.0f, /*rng_seed=*/1);
        manager->configure_links(std::move(models));
    }

    void configure_permissive_range(uint16_t first_node_id, uint16_t last_node_id) {
        std::unordered_map<uint16_t, std::unique_ptr<EntanglementAttemptNoiseInterface>> models;
        for (uint16_t a = first_node_id; a <= last_node_id; ++a) {
            for (uint16_t b = a + 1; b <= last_node_id; ++b) {
                const uint16_t key = encode_link_key(static_cast<uint8_t>(a), static_cast<uint8_t>(b));
                models[key] = std::make_unique<EntanglementAttemptModel>(/*attempt_duration_ns=*/100, /*batch_size=*/1, /*p_success=*/1.0f, /*rng_seed=*/1);
            }
        }
        manager->configure_links(std::move(models));
    }

    std::unique_ptr<CommandBase> make_entanglement_attempt(uint16_t neighbour) {
        Entangle cmd(1, neighbour, 0);
        uint8_t data_buffer[4] = {0};
        cmd.create_command(data_buffer, sizeof(data_buffer));
        return std::make_unique<EntanglementAttempt>(data_buffer);
    }

    std::unique_ptr<CommandBase> make_entanglement_measure(uint16_t neighbour) {
        EntanMeasure cmd(1, neighbour, 0, false, false, 0);
        uint8_t data_buffer[4] = {0};
        cmd.create_command(data_buffer, sizeof(data_buffer));
        return std::make_unique<EntangleMeasure>(data_buffer);
    }
};

// Test: Adding a single request
TEST_F(EntanglementManagerTest, AddSingleRequest) {
    auto cmd = make_entanglement_attempt(2);
    
    bool result = manager->add_new_request(std::move(cmd), 1, 2);
    
    EXPECT_TRUE(result);
    EXPECT_EQ(responses.size(), 0);  // No response yet
    EXPECT_EQ(commands.size(), 0);   // No command yet
}

// Test: Matching two complementary requests
TEST_F(EntanglementManagerTest, MatchComplementaryRequests) {
    // Node 1 wants to entangle with Node 2
    auto cmd1 = make_entanglement_attempt(2);
    manager->add_new_request(std::move(cmd1), 1, 2);
    
    // Node 2 wants to entangle with Node 1 - should match
    auto cmd2 = make_entanglement_attempt(1);
    bool matched = manager->add_new_request(std::move(cmd2), 2, 1);
    
    EXPECT_TRUE(matched);
    
    // Update to trigger pair processing
    manager->update_requests();
    
    // Should receive commands back (after delay)
    // Need to wait for calculated_success_time_delay
    std::this_thread::sleep_for(std::chrono::microseconds(1500));
    manager->update_requests();
    
    EXPECT_EQ(commands.size(), 2);  // Both nodes get commands back
}

// Test: Non-matching requests
TEST_F(EntanglementManagerTest, NonMatchingRequests) {
    // Node 1 wants Node 2
    auto cmd1 = make_entanglement_attempt(2);
    manager->add_new_request(std::move(cmd1), 1, 2);
    
    // Node 3 wants Node 4 - no match
    auto cmd2 = make_entanglement_attempt(4);
    manager->add_new_request(std::move(cmd2), 3, 4);
    
    EXPECT_EQ(responses.size(), 0);
    EXPECT_EQ(commands.size(), 0);
}

// Test: Request timeout
TEST_F(EntanglementManagerTest, RequestTimeout) {
    // Add a request
    auto cmd = make_entanglement_attempt(2);
    manager->add_new_request(std::move(cmd), 1, 2);
    
    // Wait for timeout (500 miliseconds) + some buffer
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    
    // Update should expire the request
    manager->update_requests();
    
    // Should get failure response
    ASSERT_EQ(responses.size(), 1);
    EXPECT_EQ(responses[0].first, 1);  // Node ID
    EXPECT_EQ(responses[0].second, Outcome_Status_t::ENTANGLEMENT_SYNC_FAILURE);
}

// Test: Multiple pending requests
TEST_F(EntanglementManagerTest, MultiplePendingRequests) {
    // Add multiple non-matching requests
    auto cmd1 = make_entanglement_attempt(2);
    auto cmd2 = make_entanglement_attempt(3);
    auto cmd3 = make_entanglement_attempt(4);
    
    manager->add_new_request(std::move(cmd1), 1, 2);
    manager->add_new_request(std::move(cmd2), 2, 3);
    manager->add_new_request(std::move(cmd3), 3, 4);
    
    // Now add matching request for first one
    auto cmd4 = make_entanglement_attempt(1);
    bool matched = manager->add_new_request(std::move(cmd4), 2, 1);
    
    EXPECT_TRUE(matched);
    // Update to trigger pair processing
    manager->update_requests();
    
    // Should receive commands back (after delay)
    // Need to wait for calculated_success_time_delay
    std::this_thread::sleep_for(std::chrono::microseconds(1500));
    manager->update_requests();
    
    EXPECT_EQ(commands.size(), 2);  // Both nodes get commands back

}

// Test: Duplicate request from same node
TEST_F(EntanglementManagerTest, DuplicateRequestHandling) {
    // First request
    auto cmd1 = make_entanglement_attempt(2);
    auto res = manager->add_new_request(std::move(cmd1), 1, 2);
    
    EXPECT_TRUE(res);
    // Duplicate request - same node to same neighbour
    auto cmd2 = make_entanglement_attempt(2);
    res = manager->add_new_request(std::move(cmd2), 1, 2);
    
    EXPECT_FALSE(res);  // Should not add duplicate
}

// Test: Update with no pending requests
TEST_F(EntanglementManagerTest, UpdateWithNoPendingRequests) {
    int result = manager->update_requests();
    
    EXPECT_EQ(result, -1);  // Nothing to process
    EXPECT_EQ(responses.size(), 0);
    EXPECT_EQ(commands.size(), 0);
}

// Test: Different command types
TEST_F(EntanglementManagerTest, DifferentCommandTypes) {
    // Attempt command
    auto attempt = make_entanglement_attempt(2);
    manager->add_new_request(std::move(attempt), 1, 2);
    
    // Measure command - should also work
    auto measure = make_entanglement_measure(1);
    bool matched = manager->add_new_request(std::move(measure), 2, 1);
    
    EXPECT_TRUE(matched);
}

// Test: Rapid successive updates
TEST_F(EntanglementManagerTest, RapidUpdates) {
    auto cmd1 = make_entanglement_attempt(2);
    auto cmd2 = make_entanglement_attempt(1);
    
    manager->add_new_request(std::move(cmd1), 1, 2);
    manager->add_new_request(std::move(cmd2), 2, 1);
    
    // Multiple rapid updates
    for (int i = 0; i < 10; ++i) {
        manager->update_requests();
    }
    
    // Should still work correctly
    EXPECT_GE(commands.size(), 0);

    // Should receive commands back (after delay)
    // Need to wait for calculated_success_time_delay
    std::this_thread::sleep_for(std::chrono::microseconds(1500));
    manager->update_requests();
    
    EXPECT_EQ(commands.size(), 2);  // Both nodes get commands back
}

// Test: Stress test with many requests
TEST_F(EntanglementManagerTest, StressTestManyRequests) {
    const int NUM_PAIRS = 50;
    
    // Add many matching pairs
    for (int i = 0; i < NUM_PAIRS; ++i) {
        auto cmd1 = make_entanglement_attempt(i * 2 + 1);
        auto cmd2 = make_entanglement_attempt(i * 2);
        
        manager->add_new_request(std::move(cmd1), i * 2, i * 2 + 1);
        manager->add_new_request(std::move(cmd2), i * 2 + 1, i * 2);
    }
    
    // Process
    std::this_thread::sleep_for(std::chrono::microseconds(1500));
    manager->update_requests();
    
    // Should have processed all pairs
    EXPECT_EQ(commands.size(), NUM_PAIRS * 2);
}

// Test: Callback is called with correct parameters
TEST_F(EntanglementManagerTest, CallbackParameters) {
    auto cmd1 = make_entanglement_attempt(2);
    auto cmd2 = make_entanglement_attempt(1);
    
    manager->add_new_request(std::move(cmd1), 5, 7);  // Node 5 -> 7
    manager->add_new_request(std::move(cmd2), 7, 5);  // Node 7 -> 5
    
    std::this_thread::sleep_for(std::chrono::microseconds(1500));
    manager->update_requests();
    
    // Check that callbacks were called with correct node IDs
    ASSERT_EQ(commands.size(), 2);
    std::vector<int> node_ids = {commands[0].first, commands[1].first};
    
    EXPECT_TRUE(std::find(node_ids.begin(), node_ids.end(), 5) != node_ids.end());
    EXPECT_TRUE(std::find(node_ids.begin(), node_ids.end(), 7) != node_ids.end());
}

// Test: Partial timeout - some requests expire while others match
TEST_F(EntanglementManagerTest, PartialTimeoutWithMatching) {
    // Add request that will timeout
    auto cmd1 = make_entanglement_attempt(2);
    manager->add_new_request(std::move(cmd1), 1, 2);
    
    // Wait for first request to timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    
    // Add new matching pair that should succeed
    auto cmd2 = make_entanglement_attempt(4);
    auto cmd3 = make_entanglement_attempt(3);
    manager->add_new_request(std::move(cmd2), 3, 4);
    manager->add_new_request(std::move(cmd3), 4, 3);
    
    // Update should expire first request and match second pair
    manager->update_requests();
    
    // Should have 1 timeout response
    ASSERT_EQ(responses.size(), 1);
    EXPECT_EQ(responses[0].first, 1);
    EXPECT_EQ(responses[0].second, Outcome_Status_t::ENTANGLEMENT_SYNC_FAILURE);
    
    // Wait for matched pair to complete
    std::this_thread::sleep_for(std::chrono::microseconds(1500));
    manager->update_requests();
    
    // Should have 2 successful commands (for the matched pair)
    EXPECT_EQ(commands.size(), 2);
}

// Test: Sequential matching - ensure FIFO behavior
TEST_F(EntanglementManagerTest, SequentialMatchingOrder) {
    // Add multiple requests from node 1 to different neighbors
    auto cmd1 = make_entanglement_attempt(2);
    auto cmd2 = make_entanglement_attempt(3);
    
    manager->add_new_request(std::move(cmd1), 1, 2);
    std::this_thread::sleep_for(std::chrono::microseconds(10)); // Small delay
    manager->add_new_request(std::move(cmd2), 1, 3);
    
    // Add matching request for the SECOND one first
    auto cmd3 = make_entanglement_attempt(1);
    bool matched = manager->add_new_request(std::move(cmd3), 3, 1);
    
    // Should match with node 1's request to node 3 (FIFO)
    EXPECT_TRUE(matched);
    
    std::this_thread::sleep_for(std::chrono::microseconds(1500));
    manager->update_requests();
    
    // Should have commands for nodes 1 and 3
    ASSERT_EQ(commands.size(), 2);
    std::vector<int> node_ids = {commands[0].first, commands[1].first};
    EXPECT_TRUE(std::find(node_ids.begin(), node_ids.end(), 1) != node_ids.end());
    EXPECT_TRUE(std::find(node_ids.begin(), node_ids.end(), 3) != node_ids.end());
}

// Test: Cascading matches - multiple matches in single update
TEST_F(EntanglementManagerTest, CascadingMatches) {
    // Setup: Add pairs 1-2, 3-4, 5-6 all at once
    std::vector<std::pair<int, int>> pairs = {{1,2}, {3,4}, {5,6}};
    
    for (const auto& [a, b] : pairs) {
        auto cmd1 = make_entanglement_attempt(b);
        auto cmd2 = make_entanglement_attempt(a);
        manager->add_new_request(std::move(cmd1), a, b);
        manager->add_new_request(std::move(cmd2), b, a);
    }
    
    // Single update should match all pairs
    manager->update_requests();
    
    // Wait for all to complete
    std::this_thread::sleep_for(std::chrono::microseconds(1500));
    manager->update_requests();
    
    // Should have 6 commands total (3 pairs * 2 nodes)
    EXPECT_EQ(commands.size(), 6);
    
    // Verify all node IDs are present
    std::set<int> node_ids;
    for (const auto& [node_id, _] : commands) {
        node_ids.insert(node_id);
    }
    EXPECT_EQ(node_ids.size(), 6);
}

// Test: Memory safety - move semantics correctness
TEST_F(EntanglementManagerTest, MoveSemanticsCorrectness) {
    auto cmd1 = make_entanglement_attempt(2);
    auto* raw_ptr1 = cmd1.get();  // Store raw pointer
    
    manager->add_new_request(std::move(cmd1), 1, 2);
    
    // Original unique_ptr should be null after move
    EXPECT_EQ(cmd1.get(), nullptr);
    
    // Add matching request
    auto cmd2 = make_entanglement_attempt(1);
    auto* raw_ptr2 = cmd2.get();
    
    manager->add_new_request(std::move(cmd2), 2, 1);
    EXPECT_EQ(cmd2.get(), nullptr);
    
    // Process
    std::this_thread::sleep_for(std::chrono::microseconds(1500));
    manager->update_requests();
    
    // Verify commands were moved to callback (not copied)
    ASSERT_EQ(commands.size(), 2);
    
    // The command pointers should be valid (not dangling)
    EXPECT_NE(commands[0].second.get(), nullptr);
    EXPECT_NE(commands[1].second.get(), nullptr);
}

// BONUS: Test edge case - same node requests entanglement with itself
TEST_F(EntanglementManagerTest, SelfEntanglementRequest) {
    // Node 1 tries to entangle with itself
    auto cmd1 = make_entanglement_attempt(1);
    bool result = manager->add_new_request(std::move(cmd1), 1, 1);

    // Should reject or handle gracefully
    EXPECT_FALSE(result);
}

// ---------------------------------------------------------------------------
// Attempt-model driven success/failure (fail-fast)
// ---------------------------------------------------------------------------

// A match on a link with no configured attempt model fails immediately --
// both sides get ENTANGLEMENT_FAILURE, no command is ever produced.
TEST_F(EntanglementManagerTest, FailFastOnMissingLinkModel) {
    auto cmd1 = make_entanglement_attempt(201);
    manager->add_new_request(std::move(cmd1), 200, 201);

    auto cmd2 = make_entanglement_attempt(200);
    bool matched = manager->add_new_request(std::move(cmd2), 201, 200);

    EXPECT_TRUE(matched); // request was handled (matched + failed), not merely queued
    ASSERT_EQ(responses.size(), 2);
    EXPECT_EQ(responses[0].second, Outcome_Status_t::ENTANGLEMENT_FAILURE);
    EXPECT_EQ(responses[1].second, Outcome_Status_t::ENTANGLEMENT_FAILURE);
    std::vector<int> failed_nodes = {responses[0].first, responses[1].first};
    EXPECT_TRUE(std::find(failed_nodes.begin(), failed_nodes.end(), 200) != failed_nodes.end());
    EXPECT_TRUE(std::find(failed_nodes.begin(), failed_nodes.end(), 201) != failed_nodes.end());

    manager->update_requests();
    EXPECT_EQ(commands.size(), 0);
}

// A match on a link whose attempt model reports batch failure (p_success=0)
// fails immediately, with no retry -- both sides get ENTANGLEMENT_FAILURE
// synchronously from add_new_request, before any update_requests() call.
TEST_F(EntanglementManagerTest, FailFastOnBatchFailure) {
    configure_failing_link(150, 151);

    auto cmd1 = make_entanglement_attempt(151);
    manager->add_new_request(std::move(cmd1), 150, 151);

    auto cmd2 = make_entanglement_attempt(150);
    manager->add_new_request(std::move(cmd2), 151, 150);

    ASSERT_EQ(responses.size(), 2);
    EXPECT_EQ(responses[0].second, Outcome_Status_t::ENTANGLEMENT_FAILURE);
    EXPECT_EQ(responses[1].second, Outcome_Status_t::ENTANGLEMENT_FAILURE);
    EXPECT_EQ(commands.size(), 0);
}

// The matched pair's execution delay comes from the attempt model's
// BatchOutcome.duration_ns, not a fixed constant -- commands aren't produced
// until that specific delay has elapsed.
TEST_F(EntanglementManagerTest, SuccessDelaySourcedFromAttemptModelDuration) {
    constexpr uint64_t kDurationNs = 20'000'000; // 20 ms
    configure_permissive_link(160, 161, kDurationNs);

    auto cmd1 = make_entanglement_attempt(161);
    manager->add_new_request(std::move(cmd1), 160, 161);
    auto cmd2 = make_entanglement_attempt(160);
    manager->add_new_request(std::move(cmd2), 161, 160);

    // Immediately after matching, the delay has not elapsed yet.
    manager->update_requests();
    EXPECT_EQ(commands.size(), 0);

    std::this_thread::sleep_for(std::chrono::nanoseconds(kDurationNs) + std::chrono::milliseconds(10));
    manager->update_requests();
    EXPECT_EQ(commands.size(), 2);
}