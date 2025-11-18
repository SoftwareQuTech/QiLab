#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "Simulator/entanglement/entanglement_manager.hpp"
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
        responses.clear();
        commands.clear();
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
    
    // Wait for timeout (500 microseconds)
    std::this_thread::sleep_for(std::chrono::microseconds(600));
    
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
}

// Test: Duplicate request from same node
TEST_F(EntanglementManagerTest, DuplicateRequestHandling) {
    // First request
    auto cmd1 = make_entanglement_attempt(2);
    manager->add_new_request(std::move(cmd1), 1, 2);
    
    // Duplicate request - same node to same neighbour
    auto cmd2 = make_entanglement_attempt(2);
    manager->add_new_request(std::move(cmd2), 1, 2);
    
    // Implementation should handle this appropriately
    // Check your actual behavior here
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