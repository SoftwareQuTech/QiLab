// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

/*
** Unit tests for SimulatorRunner (src/Simulator/SimulatorRunners/simulator_runner.*).
**
** SimulatorRunner is a thread-loop coordinator: on_run() blocks, polling a
** MessageBroker for commands destined to DestinationType::SimRunner,
** dispatching them to a SimulatorManager, and routing responses back via the
** broker, until on_abort() is called from another thread.
**
** Per project decision, this is tested with its REAL, lightweight
** collaborators (MessageBroker, SimulatorManager) rather than mocks -- it's
** still "one class under test," just accepting real collaborators instead of
** gmock doubles. A minimal ISimulator stub stands in for the actual Stim
** adapter, since adapter internals are out of scope here.
**
** Threading methodology (see RunnerThreadGuard / wait_for_response below):
**  - on_run() is NEVER called on the test thread directly; it always runs on
**    a dedicated std::thread, with core_id=-1 so it never attempts real CPU
**    pinning (out of scope / not reliably unit-testable per project
**    conventions around utils/thread_utils.hpp).
**  - on_abort() + thread.join() are performed by an RAII guard so a failed
**    ASSERT_* mid-test can never leak a running thread into later tests.
**  - Responses are awaited with a bounded poll loop (small sleep, capped
**    iteration count), never a fixed sleep-then-assert, to avoid both
**    flakiness under CI load and true hangs on a real bug.
*/

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "DataTypes/commands.hpp"
#include "DataTypes/responses.hpp"
#include "DataTypes/configuration_options.hpp"
#include "MessageBroker/message_broker.hpp"
#include "Simulator/sim_interface.hpp"
#include "Simulator/SimulatorManager/simulator_manager.hpp"
#include "Simulator/SimulatorRunners/simulator_runner.hpp"
#include "Simulator/NoiseModels/QubitDecoherenceNoise/qubit_noise_basic.hpp"
#include "mocks/command_mock.hpp"

namespace
{

// ---- Minimal ISimulator stub -------------------------------------------
//
// The runner's behavior under test is command routing/threading, not
// simulator internals (SimulatorManager and the real adapter already have
// their own dedicated tests). This stub just succeeds unconditionally so
// SimulatorManager's handlers take their success paths.
class StubSimulator : public ISimulator
{
public:
    bool qubit_initialize(uint8_t) override { return true; }
    bool single_qubit_gate(uint8_t, Angle_t, Axis_t) override { return true; }
    bool single_qubit_noise(uint8_t, float, float, float) override { return true; }
    bool two_qubit_depolarizing_noise(uint8_t, uint8_t, float) override { return true; }
    bool two_qubit_gate(uint8_t, uint8_t, Angle_t, Axis_t) override { return true; }
    uint8_t qubit_measure(uint8_t, bool) override { return 0; }
    int8_t attempt_entanglement(uint16_t, uint8_t, uint8_t, bool, uint8_t) override { return 0; }
    bool qubit_move(uint8_t, uint8_t) override { return true; }
    bool qubit_swap(uint8_t, uint8_t) override { return true; }
    bool entanglement_swap(uint8_t, uint8_t) override { return true; }
    bool allocate_qubits(uint8_t) override { return true; }
};

// ---- Helpers -------------------------------------------------------------

NodeConfig make_node_config(int num_qubits)
{
    NodeConfig cfg;
    cfg.num_qubits = num_qubits;
    cfg.name = "node";
    cfg.qubit_operations_delays = QuantumOperationsDelaysNs{}; // all-zero delays
    QubitArrayConfig array;
    array.num_qubits = num_qubits;
    array.qubit_decoherence_noise_model.config_data = QubitNoiseBasicConfig{1000.0f};
    array.single_gate_delay_ns = 0;
    array.gate_noise_model.config_data = GateNoiseDepolarizingConfig{0.0f};
    cfg.qubit_arrays.push_back(array);
    return cfg;
}

// Mirrors the BaseC -> concrete CommandBase decoding used by
// test/Functional/Simulator/SimulatorManager/test_simulator_manager.cpp: builds a
// wire-format command via the mocks/command_mock.hpp builders, then decodes it
// into the matching CommandBase-derived type the same way the real
// command-parsing path would.
std::unique_ptr<CommandBase> build_command(const BaseC &builder)
{
    uint8_t buf[8] = {0};
    builder.create_command(buf, sizeof(buf));
    switch (static_cast<Command_t>(buf[0]))
    {
    case Command_t::NOP:
        return std::make_unique<NOP_Command>(buf);
    case Command_t::QUBIT_INIT:
        return std::make_unique<QubitInitialize>(buf);
    default:
        return nullptr;
    }
}

TransportData<CommandBase> make_command_transport(uint16_t node_id, std::unique_ptr<CommandBase> cmd)
{
    TransportData<CommandBase> transport;
    transport.source = DestinationType::CommandRunner;
    transport.destination = DestinationType::SimRunner;
    transport.node_id = node_id;
    transport.transport = std::move(cmd);
    return transport;
}

// Bounded poll for a response: small sleep, capped iteration count. Default
// caps total wait at ~1 second (200 * 5ms), matching the "never a fixed sleep,
// never an unbounded hang" methodology for thread-based tests.
bool wait_for_response(MessageBroker &broker, TransportData<ResponseData> &out,
                       int max_iters = 200, std::chrono::milliseconds sleep_per_iter = std::chrono::milliseconds(5))
{
    for (int i = 0; i < max_iters; ++i)
    {
        if (broker.get_response(out))
        {
            return true;
        }
        std::this_thread::sleep_for(sleep_per_iter);
    }
    return false;
}

// RAII guard: always signals on_abort() and joins the runner thread, even if
// an ASSERT_* fails mid-test. This is the ONLY place on_run() is invoked, and
// it is always on a dedicated thread (never the test's main thread), with
// core_id=-1 so no real CPU pinning is attempted.
struct RunnerThreadGuard
{
    explicit RunnerThreadGuard(SimulatorRunner &r)
        : runner(r), thread([&r] { r.on_run(-1); })
    {
    }

    ~RunnerThreadGuard()
    {
        runner.on_abort();
        if (thread.joinable())
        {
            thread.join();
        }
    }

    SimulatorRunner &runner;
    std::thread thread;
};

// ---- Fixture ---------------------------------------------------------------

class SimulatorRunnerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        broker = std::make_shared<MessageBroker>();
    }

    // Builds a SimulatorRunner wrapping a fresh SimulatorManager (with a
    // StubSimulator adapter) that has `num_nodes` nodes registered, each with
    // `qubits_per_node` qubits and zero operation delays (so responses become
    // ready almost immediately, keeping the bounded polls fast).
    std::unique_ptr<SimulatorRunner> make_runner(int num_nodes, int qubits_per_node)
    {
        auto sim = std::make_unique<StubSimulator>();
        auto mgr = std::make_unique<SimulatorManager>(std::move(sim));
        for (int i = 0; i < num_nodes; ++i)
        {
            EXPECT_EQ(mgr->register_node(make_node_config(qubits_per_node)), i);
        }
        return std::make_unique<SimulatorRunner>(std::move(mgr), broker);
    }

    std::shared_ptr<MessageBroker> broker;
};

// ---- Tests -----------------------------------------------------------------

// A single NOP command routed in via the broker is picked up by the runner,
// processed by the SimulatorManager, and a SUCCESS response is routed back
// out, observable via get_response().
TEST_F(SimulatorRunnerTest, SingleNopCommandIsProcessedAndResponseRouted)
{
    auto runner = make_runner(/*num_nodes=*/1, /*qubits_per_node=*/1);
    RunnerThreadGuard guard(*runner);

    ASSERT_TRUE(broker->route_command(make_command_transport(0, build_command(Nop()))));

    TransportData<ResponseData> response;
    ASSERT_TRUE(wait_for_response(*broker, response)) << "No response received within the bounded wait";

    EXPECT_EQ(response.node_id, 0);
    EXPECT_EQ(response.source, DestinationType::SimRunner);
    EXPECT_EQ(response.destination, DestinationType::CommandRunner);
    ASSERT_NE(response.transport, nullptr);
    EXPECT_TRUE(response.transport->outcome & static_cast<int>(Outcome_Status_t::SUCCESS));
    EXPECT_TRUE(response.transport->status & static_cast<int>(NodeStatus_t::OUTCOME_READY));
}

// Same round trip with a QubitInitialize command (a second command type),
// exercising the runner's dispatch path through a handler that actually
// touches a real qubit address rather than the NOP no-op path.
TEST_F(SimulatorRunnerTest, QubitInitializeCommandProducesSuccessResponse)
{
    auto runner = make_runner(/*num_nodes=*/1, /*qubits_per_node=*/2);
    RunnerThreadGuard guard(*runner);

    ASSERT_TRUE(broker->route_command(make_command_transport(0, build_command(InitQubit(/*addr=*/1)))));

    TransportData<ResponseData> response;
    ASSERT_TRUE(wait_for_response(*broker, response)) << "No response received within the bounded wait";

    EXPECT_EQ(response.node_id, 0);
    ASSERT_NE(response.transport, nullptr);
    EXPECT_TRUE(response.transport->outcome & static_cast<int>(Outcome_Status_t::SUCCESS));
}

// Multiple commands routed in sequence, one per node, are all processed and
// produce responses whose node_id ordering matches the order commands were
// sent -- the broker's queue is FIFO per destination (see
// test/Unit/MessageBroker/test_message_broker.cpp FIFOOrderSameDestination),
// and the runner processes at most one command and attempts at most one
// ready-response fetch per loop iteration, so ordering is preserved end to
// end for same-type/zero-delay commands.
TEST_F(SimulatorRunnerTest, MultipleCommandsAreProcessedInOrder)
{
    constexpr int NUM_COMMANDS = 3;
    auto runner = make_runner(/*num_nodes=*/NUM_COMMANDS, /*qubits_per_node=*/1);
    RunnerThreadGuard guard(*runner);

    for (uint16_t node_id = 0; node_id < NUM_COMMANDS; ++node_id)
    {
        ASSERT_TRUE(broker->route_command(make_command_transport(node_id, build_command(Nop()))));
    }

    std::vector<uint16_t> received_node_ids;
    for (int i = 0; i < NUM_COMMANDS; ++i)
    {
        TransportData<ResponseData> response;
        ASSERT_TRUE(wait_for_response(*broker, response)) << "Missing response #" << i;
        ASSERT_NE(response.transport, nullptr);
        EXPECT_TRUE(response.transport->outcome & static_cast<int>(Outcome_Status_t::SUCCESS));
        received_node_ids.push_back(response.node_id);
    }

    EXPECT_EQ(received_node_ids, (std::vector<uint16_t>{0, 1, 2}));
}

// on_abort() must stop the loop promptly: on_run()'s loop only sleeps 5
// microseconds per iteration between checks of the abort flag, so a working
// implementation should return well within tens of milliseconds. A regular
// join() is used (not a watchdog with a timeout): if on_abort() were
// genuinely broken (e.g. never observed by the loop), this test hanging is
// itself the correct failure signal, since destroying the runner/manager
// while on_run() is still executing would be undefined behavior regardless.
TEST_F(SimulatorRunnerTest, OnAbortStopsLoopPromptly)
{
    auto runner = make_runner(/*num_nodes=*/1, /*qubits_per_node=*/1);
    RunnerThreadGuard guard(*runner);

    // Let the loop actually start running a few iterations before aborting.
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    const auto start = std::chrono::steady_clock::now();
    runner->on_abort();
    ASSERT_TRUE(guard.thread.joinable());
    guard.thread.join();
    const auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_LT(elapsed, std::chrono::milliseconds(500))
        << "on_abort() should stop the runner loop promptly (loop only sleeps 5us/iteration)";
    // guard's destructor will run at scope exit too: on_abort() is idempotent
    // (just re-sets the flag) and thread.joinable() is now false since we
    // already joined above, so it is a safe no-op.
}

// A command for an unregistered/invalid node must not crash the runner
// thread or leave it stuck.
//
// NOTE on current behavior (see simulator_manager.cpp
// SimulatorManager::add_new_command_to_process): for node_id >=
// registered_nodes, it populates a local ResponseData with
// outcome=INVALID_QUBIT/status=READY|OUTCOME_READY but returns -2 *before*
// calling operations_engine.add_new_response_to_wait(). SimulatorRunner::on_run
// only logs that non-zero return code (LOG_ERROR) and never routes that
// ResponseData back through the broker -- it only routes whatever
// SimulatorManager::try_get_ready_response() (backed by the operations-engine
// queue) later reports as ready, which this rejected command never entered.
// So today, an invalid-node command's "failure response" is silently dropped
// rather than delivered to CommandRunner. This test documents that actual
// behavior (no response observed) while asserting the more important
// invariant: the runner thread survives and keeps servicing later valid
// commands normally. If SimulatorManager/SimulatorRunner is later changed to
// route a failure response for invalid nodes, update this test to assert on
// that response's content (e.g. INVALID_QUBIT) instead of its absence.
TEST_F(SimulatorRunnerTest, InvalidNodeIdCommandDoesNotCrashRunner)
{
    auto runner = make_runner(/*num_nodes=*/1, /*qubits_per_node=*/1); // only node_id 0 is valid
    RunnerThreadGuard guard(*runner);

    constexpr uint16_t INVALID_NODE_ID = 7;
    ASSERT_TRUE(broker->route_command(make_command_transport(INVALID_NODE_ID, build_command(Nop()))));

    TransportData<ResponseData> response;
    EXPECT_FALSE(wait_for_response(*broker, response, /*max_iters=*/60, std::chrono::milliseconds(5)))
        << "Unexpectedly received a routed response for an invalid node id -- "
           "SimulatorManager/SimulatorRunner behavior may have changed; see the "
           "comment on this test.";

    // The runner thread must still be healthy: a subsequent valid command is
    // processed normally.
    ASSERT_TRUE(broker->route_command(make_command_transport(0, build_command(Nop()))));
    TransportData<ResponseData> valid_response;
    ASSERT_TRUE(wait_for_response(*broker, valid_response))
        << "Runner failed to process a valid command after an invalid one";
    EXPECT_EQ(valid_response.node_id, 0);
    ASSERT_NE(valid_response.transport, nullptr);
    EXPECT_TRUE(valid_response.transport->outcome & static_cast<int>(Outcome_Status_t::SUCCESS));
}

// With no commands ever routed in, the loop must not fabricate/route any
// response, and must still abort cleanly -- covering the "idle" branch of
// the loop (get_command() returns false, try_get_ready_response() returns
// nonzero every iteration) alongside the core_id=-1 no-pin construction path
// exercised implicitly by every test in this file.
TEST_F(SimulatorRunnerTest, IdleRunnerProducesNoResponseAndAbortsCleanly)
{
    auto runner = make_runner(/*num_nodes=*/1, /*qubits_per_node=*/1);
    RunnerThreadGuard guard(*runner);

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    TransportData<ResponseData> response;
    EXPECT_FALSE(broker->get_response(response)) << "Idle runner must not route spurious responses";
}

} // namespace
