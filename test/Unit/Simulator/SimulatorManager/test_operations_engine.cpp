// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <chrono>
#include <thread>

#include <gtest/gtest.h>

#include "Simulator/SimulatorManager/operations_engine.hpp"

TEST(OperationsEngineTest, ImmediateResponseIsReady)
{
    OperationsEngine engine;

    ResponseData in;
    in.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
    in.outcome = Outcome_Status_t::SUCCESS;
    in.cycle_count = 42;

    ASSERT_TRUE(engine.add_new_response_to_wait(in, 3, 0));

    ResponseData out;
    uint16_t node_id = 0;
    ASSERT_TRUE(engine.try_get_ready_response(out, node_id));
    EXPECT_EQ(node_id, 3);
    EXPECT_EQ(out.status, in.status);
    EXPECT_EQ(out.outcome, in.outcome);
    EXPECT_EQ(out.cycle_count, in.cycle_count);
}

TEST(OperationsEngineTest, DelayedResponseBecomesReadyAfterWait)
{
    OperationsEngine engine;

    ResponseData in;
    in.outcome = Outcome_Status_t::SUCCESS;

    ASSERT_TRUE(engine.add_new_response_to_wait(in, 1, 2'000'000)); // 2 ms

    ResponseData out;
    uint16_t node_id = 0;
    EXPECT_FALSE(engine.try_get_ready_response(out, node_id));

    std::this_thread::sleep_for(std::chrono::milliseconds(3));

    ASSERT_TRUE(engine.try_get_ready_response(out, node_id));
    EXPECT_EQ(node_id, 1);
    EXPECT_EQ(out.outcome, in.outcome);
}

TEST(OperationsEngineTest, ReturnsEarliestReadyOutOfOrder)
{
    OperationsEngine engine;

    ResponseData slow;
    slow.cycle_count = 10;
    ResponseData fast;
    fast.cycle_count = 20;

    ASSERT_TRUE(engine.add_new_response_to_wait(slow, 7, 5'000'000)); // 5 ms
    ASSERT_TRUE(engine.add_new_response_to_wait(fast, 9, 1'000'000)); // 1 ms

    std::this_thread::sleep_for(std::chrono::milliseconds(2));

    ResponseData out;
    uint16_t node_id = 0;
    ASSERT_TRUE(engine.try_get_ready_response(out, node_id));
    EXPECT_EQ(node_id, 9);
    EXPECT_EQ(out.cycle_count, 20);

    EXPECT_FALSE(engine.try_get_ready_response(out, node_id));

    std::this_thread::sleep_for(std::chrono::milliseconds(4));

    ASSERT_TRUE(engine.try_get_ready_response(out, node_id));
    EXPECT_EQ(node_id, 7);
    EXPECT_EQ(out.cycle_count, 10);
}

TEST(OperationsEngineTest, HandlesBurstWithoutLosingResponses)
{
    OperationsEngine engine;

    constexpr uint16_t kCount = 80;
    for (uint16_t i = 0; i < kCount; ++i)
    {
        ResponseData r;
        r.cycle_count = i;
        ASSERT_TRUE(engine.add_new_response_to_wait(r, i, 0));
    }

    uint16_t drained = 0;
    ResponseData out;
    uint16_t node_id = 0;
    while (engine.try_get_ready_response(out, node_id))
    {
        ++drained;
    }

    EXPECT_EQ(drained, kCount);
}
