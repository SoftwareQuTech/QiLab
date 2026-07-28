/*
** Full-pipeline functional tests: one non-entanglement operation type per
** TEST, driven through the real MessageBroker + SimulatorRunner +
** SimulatorManager stack (a StubSimulator stands in for the real Stim
** adapter, since this suite is about pipeline routing/dispatch correctness,
** not quantum validity -- that's covered separately by the Stim adapter unit
** tests and the statistical validation suite).
**
** This complements (does not duplicate) test/Unit/Simulator/SimulatorRunners/
** test_simulator_runner.cpp, which already covers NOP/QubitInitialize plus
** threading/idle/invalid-node behavior -- this file extends the same
** RAII-guard + bounded-polling methodology to every remaining single-node
** command type, success and invalid-qubit failure paths, plus a pinned
** regression test for the "unimplemented command type" branch.
*/

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <thread>

#include "DataTypes/commands.hpp"
#include "DataTypes/responses.hpp"
#include "DataTypes/configuration_options.hpp"
#include "MessageBroker/message_broker.hpp"
#include "Simulator/sim_interface.hpp"
#include "Simulator/SimulatorManager/simulator_manager.hpp"
#include "Simulator/SimulatorRunners/simulator_runner.hpp"
#include "Simulator/NoiseModels/QubitDecoherenceNoise/qubit_noise_basic.hpp"
#include "mocks/command_mock.hpp"

namespace {

class StubSimulator : public ISimulator {
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

NodeConfig make_node_config(int num_qubits) {
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

// Decodes every non-entanglement command type this suite exercises. Mirrors
// TransmissionProcessor::deserialize_command's switch (see
// src/DataTransmission/TransmissionProcessor/transmission_processor.cpp),
// minus the CRC-framing step -- commands here are handed to the runner
// pre-decoded via route_command, same as test_simulator_runner.cpp does.
std::unique_ptr<CommandBase> build_command(const BaseC &builder) {
    uint8_t buf[8] = {0};
    builder.create_command(buf, sizeof(buf));
    switch (static_cast<Command_t>(buf[0])) {
        case Command_t::NOP: return std::make_unique<NOP_Command>(buf);
        case Command_t::QUBIT_INIT: return std::make_unique<QubitInitialize>(buf);
        case Command_t::SINGLE_QUBIT_GATE: return std::make_unique<SingleQubitGate>(buf);
        case Command_t::TWO_QUBIT_GATE: return std::make_unique<TwoQubitGate>(buf);
        case Command_t::QUBIT_MEASUREMENT: return std::make_unique<QubitMeasure>(buf);
        case Command_t::PRE_MEASUREMENT_GATES_SELECTION: return std::make_unique<PreMeasureGatesSelection>(buf);
        case Command_t::QUBIT_MOVE: return std::make_unique<QubitMove>(buf);
        case Command_t::QUBIT_SWAP: return std::make_unique<QubitSwap>(buf);
        default: return nullptr;
    }
}

TransportData<CommandBase> make_command_transport(uint16_t node_id, std::unique_ptr<CommandBase> cmd) {
    TransportData<CommandBase> transport;
    transport.source = DestinationType::CommandRunner;
    transport.destination = DestinationType::SimRunner;
    transport.node_id = node_id;
    transport.transport = std::move(cmd);
    return transport;
}

bool wait_for_response(MessageBroker &broker, TransportData<ResponseData> &out,
                        int max_iters = 200, std::chrono::milliseconds sleep_per_iter = std::chrono::milliseconds(5)) {
    for (int i = 0; i < max_iters; ++i) {
        if (broker.get_response(out)) return true;
        std::this_thread::sleep_for(sleep_per_iter);
    }
    return false;
}

struct RunnerThreadGuard {
    explicit RunnerThreadGuard(SimulatorRunner &r) : runner(r), thread([&r] { r.on_run(-1); }) {}
    ~RunnerThreadGuard() {
        runner.on_abort();
        if (thread.joinable()) thread.join();
    }
    SimulatorRunner &runner;
    std::thread thread;
};

class FullPipelineSingleNodeOpsTest : public ::testing::Test {
protected:
    void SetUp() override { broker = std::make_shared<MessageBroker>(); }

    std::unique_ptr<SimulatorRunner> make_runner(int num_nodes, int qubits_per_node) {
        auto sim = std::make_unique<StubSimulator>();
        auto mgr = std::make_unique<SimulatorManager>(std::move(sim));
        for (int i = 0; i < num_nodes; ++i) {
            EXPECT_EQ(mgr->register_node(make_node_config(qubits_per_node)), i);
        }
        return std::make_unique<SimulatorRunner>(std::move(mgr), broker);
    }

    std::shared_ptr<MessageBroker> broker;
};

TEST_F(FullPipelineSingleNodeOpsTest, SingleQubitGateSucceedsForValidQubit) {
    auto runner = make_runner(1, 2);
    RunnerThreadGuard guard(*runner);

    ASSERT_TRUE(broker->route_command(make_command_transport(
        0, build_command(SingleQGate(/*addr=*/1, Angle_t::_9_PI_16, Axis_t::Y_AXIS)))));

    TransportData<ResponseData> response;
    ASSERT_TRUE(wait_for_response(*broker, response));
    ASSERT_NE(response.transport, nullptr);
    EXPECT_TRUE(response.transport->outcome & static_cast<int>(Outcome_Status_t::SUCCESS));
}

TEST_F(FullPipelineSingleNodeOpsTest, SingleQubitGateFailsForOutOfRangeQubit) {
    auto runner = make_runner(1, 2); // only qubit addresses 0-1 are valid
    RunnerThreadGuard guard(*runner);

    ASSERT_TRUE(broker->route_command(make_command_transport(
        0, build_command(SingleQGate(/*addr=*/9, Angle_t::_PI_16, Axis_t::X_AXIS)))));

    TransportData<ResponseData> response;
    ASSERT_TRUE(wait_for_response(*broker, response));
    ASSERT_NE(response.transport, nullptr);
    EXPECT_EQ(response.transport->outcome, Outcome_Status_t::INVALID_QUBIT);
}

TEST_F(FullPipelineSingleNodeOpsTest, TwoQubitGateSucceedsForValidQubitPair) {
    auto runner = make_runner(1, 2);
    RunnerThreadGuard guard(*runner);

    ASSERT_TRUE(broker->route_command(make_command_transport(
        0, build_command(TwoQGate(/*q1=*/0, /*q2=*/1, Angle_t::_8_PI_16, Axis_t::X_AXIS)))));

    TransportData<ResponseData> response;
    ASSERT_TRUE(wait_for_response(*broker, response));
    ASSERT_NE(response.transport, nullptr);
    EXPECT_TRUE(response.transport->outcome & static_cast<int>(Outcome_Status_t::SUCCESS));
}

TEST_F(FullPipelineSingleNodeOpsTest, TwoQubitGateFailsForOutOfRangeQubit) {
    auto runner = make_runner(1, 2);
    RunnerThreadGuard guard(*runner);

    ASSERT_TRUE(broker->route_command(make_command_transport(
        0, build_command(TwoQGate(/*q1=*/0, /*q2=*/9, Angle_t::_8_PI_16, Axis_t::X_AXIS)))));

    TransportData<ResponseData> response;
    ASSERT_TRUE(wait_for_response(*broker, response));
    ASSERT_NE(response.transport, nullptr);
    EXPECT_EQ(response.transport->outcome, Outcome_Status_t::INVALID_QUBIT);
}

TEST_F(FullPipelineSingleNodeOpsTest, QubitMeasurementSucceedsForValidQubit) {
    auto runner = make_runner(1, 1);
    RunnerThreadGuard guard(*runner);

    ASSERT_TRUE(broker->route_command(make_command_transport(
        0, build_command(QMeasure(/*addr=*/0, /*basis=*/false, Axis_t::Z_AXIS)))));

    TransportData<ResponseData> response;
    ASSERT_TRUE(wait_for_response(*broker, response));
    ASSERT_NE(response.transport, nullptr);
    EXPECT_TRUE(response.transport->outcome & static_cast<int>(Outcome_Status_t::SUCCESS));
}

TEST_F(FullPipelineSingleNodeOpsTest, QubitMeasurementFailsForOutOfRangeQubit) {
    auto runner = make_runner(1, 1);
    RunnerThreadGuard guard(*runner);

    ASSERT_TRUE(broker->route_command(make_command_transport(
        0, build_command(QMeasure(/*addr=*/5, /*basis=*/false, Axis_t::Z_AXIS)))));

    TransportData<ResponseData> response;
    ASSERT_TRUE(wait_for_response(*broker, response));
    ASSERT_NE(response.transport, nullptr);
    EXPECT_EQ(response.transport->outcome, Outcome_Status_t::INVALID_QUBIT);
}

TEST_F(FullPipelineSingleNodeOpsTest, QubitMoveSucceedsForValidQubitPair) {
    auto runner = make_runner(1, 2);
    RunnerThreadGuard guard(*runner);

    ASSERT_TRUE(broker->route_command(make_command_transport(
        0, build_command(QMove(/*q1=*/0, /*q2=*/1)))));

    TransportData<ResponseData> response;
    ASSERT_TRUE(wait_for_response(*broker, response));
    ASSERT_NE(response.transport, nullptr);
    EXPECT_TRUE(response.transport->outcome & static_cast<int>(Outcome_Status_t::SUCCESS));
}

TEST_F(FullPipelineSingleNodeOpsTest, QubitMoveFailsForOutOfRangeQubit) {
    auto runner = make_runner(1, 2);
    RunnerThreadGuard guard(*runner);

    ASSERT_TRUE(broker->route_command(make_command_transport(
        0, build_command(QMove(/*q1=*/0, /*q2=*/9)))));

    TransportData<ResponseData> response;
    ASSERT_TRUE(wait_for_response(*broker, response));
    ASSERT_NE(response.transport, nullptr);
    EXPECT_EQ(response.transport->outcome, Outcome_Status_t::INVALID_QUBIT);
}

TEST_F(FullPipelineSingleNodeOpsTest, QubitSwapSucceedsForValidQubitPair) {
    auto runner = make_runner(1, 2);
    RunnerThreadGuard guard(*runner);

    ASSERT_TRUE(broker->route_command(make_command_transport(
        0, build_command(QSwap(/*q1=*/0, /*q2=*/1)))));

    TransportData<ResponseData> response;
    ASSERT_TRUE(wait_for_response(*broker, response));
    ASSERT_NE(response.transport, nullptr);
    EXPECT_TRUE(response.transport->outcome & static_cast<int>(Outcome_Status_t::SUCCESS));
}

TEST_F(FullPipelineSingleNodeOpsTest, QubitSwapFailsForOutOfRangeQubit) {
    auto runner = make_runner(1, 2);
    RunnerThreadGuard guard(*runner);

    ASSERT_TRUE(broker->route_command(make_command_transport(
        0, build_command(QSwap(/*q1=*/0, /*q2=*/9)))));

    TransportData<ResponseData> response;
    ASSERT_TRUE(wait_for_response(*broker, response));
    ASSERT_NE(response.transport, nullptr);
    EXPECT_EQ(response.transport->outcome, Outcome_Status_t::INVALID_QUBIT);
}

TEST_F(FullPipelineSingleNodeOpsTest, PreMeasurementGatesSelectionSucceeds) {
    auto runner = make_runner(1, 1);
    RunnerThreadGuard guard(*runner);

    ASSERT_TRUE(broker->route_command(make_command_transport(
        0, build_command(PreMeasureGateSelect(Angle_t::_8_PI_16, Angle_t::_8_PI_16, Angle_t::_8_PI_16, /*target=*/false)))));

    TransportData<ResponseData> response;
    ASSERT_TRUE(wait_for_response(*broker, response));
    ASSERT_NE(response.transport, nullptr);
    EXPECT_TRUE(response.transport->outcome & static_cast<int>(Outcome_Status_t::SUCCESS));
}

// Pinned regression: an unimplemented/out-of-range command type (here,
// PRE_MEASURE_AND_ENTANGLE_MEASURE, which has an enum value but no entry in
// SimulatorManager's sim_command_table) hits the -3 branch in
// add_new_command_to_process, which -- like the -2 invalid-node-id branch
// covered in test_simulator_runner.cpp -- never writes a response into the
// operations-engine queue, so SimulatorRunner never routes anything back.
// This pins down that a VALID node with an unimplemented command type also
// gets its response silently dropped rather than an error status, which is a
// distinct code path from the invalid-node-id case (still worth confirming
// separately, since the two branches could diverge independently in future
// changes). If this is ever fixed to route a failure response, update this
// test to assert on that response's content instead of its absence.
TEST_F(FullPipelineSingleNodeOpsTest, UnimplementedCommandTypeProducesNoResponse) {
    auto runner = make_runner(1, 1);
    RunnerThreadGuard guard(*runner);

    uint8_t raw[4] = {static_cast<uint8_t>(Command_t::PRE_MEASURE_AND_ENTANGLE_MEASURE), 0, 0, 0};
    ASSERT_TRUE(broker->route_command(make_command_transport(0, std::make_unique<NOP_Command>(raw))));
    // NOP_Command just stores command_type=data[0]=PRE_MEASURE_AND_ENTANGLE_MEASURE
    // and no extra fields, which is all add_new_command_to_process's dispatch
    // table lookup needs to hit the unimplemented-type branch.

    TransportData<ResponseData> response;
    EXPECT_FALSE(wait_for_response(*broker, response, /*max_iters=*/60, std::chrono::milliseconds(5)))
        << "Unexpectedly received a routed response for an unimplemented command type -- "
           "SimulatorManager behavior may have changed; see the comment on this test.";
}

} // namespace
