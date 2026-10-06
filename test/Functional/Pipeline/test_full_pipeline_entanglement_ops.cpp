// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

/*
** Full-pipeline functional tests for entanglement commands: EntanglementRunner
** and SimulatorRunner sharing ONE real MessageBroker. This is the genuinely
** new cross-runner behavior not covered by either runner's own unit test:
** test/Unit/Simulator/SimulatorRunners/test_entanglement_runner.cpp only
** confirms a matched attempt's forwarded command REACHES SimRunner's queue
** (via a bare get_command poll) -- it never pairs with a live SimulatorRunner
** to see the real final response a node would actually receive. This suite
** closes that gap: a raw entanglement command goes in via the broker, and a
** genuine SUCCESS/FAILURE response -- produced by a live SimulatorManager via
** a live SimulatorRunner -- comes back out.
**
** The regression test for the ENTANGLEMENT_SWAP decode-bounds bug (fixed in
** transmission_processor.cpp) lives at the more precise TransmissionProcessor
** unit level (test_transmission_processor_simple.cpp,
** ReceivesEntanglementSwap) since that's exactly where the bug was; here we
** only need to confirm ENTANGLEMENT_SWAP -- which SimRunner handles directly,
** not via EntanglementRunner -- still gets a normal response through
** SimulatorRunner alone (no pairing/matching involved for this op type).
*/

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <thread>
#include <unordered_map>

#include "DataTypes/commands.hpp"
#include "DataTypes/responses.hpp"
#include "DataTypes/configuration_options.hpp"
#include "MessageBroker/message_broker.hpp"
#include "Simulator/sim_interface.hpp"
#include "Simulator/SimulatorManager/simulator_manager.hpp"
#include "Simulator/SimulatorRunners/simulator_runner.hpp"
#include "Simulator/SimulatorRunners/entanglement_runner.hpp"
#include "Simulator/NoiseModels/QubitDecoherenceNoise/qubit_noise_basic.hpp"
#include "Simulator/NoiseModels/EntanglementGenerationNoise/entanglement_attempt_noise.hpp"
#include "Simulator/entanglement_attempt_noise_interface.hpp"
#include "Simulator/link_key.hpp"
#include "mocks/command_mock.hpp"

using AttemptModelsMap = std::unordered_map<uint16_t, std::unique_ptr<EntanglementAttemptNoiseInterface>>;

namespace {

class StubSimulator : public ISimulator {
public:
    bool qubit_initialize(uint8_t) override { return true; }
    bool single_qubit_gate(uint8_t, Angle_t, Axis_t) override { return true; }
    bool single_qubit_noise(uint8_t, float, float, float) override { return true; }
    bool two_qubit_depolarizing_noise(uint8_t, uint8_t, float) override { return true; }
    bool two_qubit_gate(uint8_t, uint8_t, Angle_t, Axis_t) override { return true; }
    uint8_t qubit_measure(uint8_t, bool) override { return 0; }
    int8_t attempt_entanglement(uint16_t, uint8_t, uint8_t, bool is_initiator, uint8_t) override {
        return is_initiator ? 0 : 0; // STATE_PHI_PLUS (0) for both sides
    }
    bool qubit_move(uint8_t, uint8_t) override { return true; }
    bool qubit_swap(uint8_t, uint8_t) override { return true; }
    bool entanglement_swap(uint8_t, uint8_t) override { return true; }
    bool allocate_qubits(uint8_t) override { return true; }
};

NodeConfig make_node_config(int num_qubits) {
    NodeConfig cfg;
    cfg.num_qubits = num_qubits;
    cfg.name = "node";
    cfg.qubit_operations_delays = QuantumOperationsDelaysNs{};
    QubitArrayConfig array;
    array.num_qubits = num_qubits;
    array.qubit_decoherence_noise_model.config_data = QubitNoiseBasicConfig{1000.0f};
    array.single_gate_delay_ns = 0;
    array.gate_noise_model.config_data = GateNoiseDepolarizingConfig{0.0f};
    cfg.qubit_arrays.push_back(array);
    return cfg;
}

// See test/Unit/Simulator/SimulatorRunners/test_entanglement_runner.cpp: nghbr
// decodes as ((NGHBR_BIT_MASK & data[1]) >> 4) - 1, so the wire value must be
// neighbour+1 (only round-trips for neighbour <= 14).
std::unique_ptr<CommandBase> make_entanglement_attempt_cmd(uint8_t neighbour) {
    Entangle cmd(/*fidelity=*/1, static_cast<uint8_t>(neighbour + 1), /*qid=*/0);
    uint8_t data_buffer[4] = {0};
    cmd.create_command(data_buffer, sizeof(data_buffer));
    return std::make_unique<EntanglementAttempt>(data_buffer);
}

AttemptModelsMap make_permissive_link(uint8_t node_a, uint8_t node_b, uint64_t duration_ns = 100) {
    AttemptModelsMap models;
    const uint16_t key = encode_link_key(node_a, node_b);
    models[key] = std::make_unique<EntanglementAttemptModel>(duration_ns, /*batch_size=*/1, /*p_success=*/1.0f, /*rng_seed=*/1);
    return models;
}

bool push_entanglement_command(MessageBroker &broker, uint16_t node_id, std::unique_ptr<CommandBase> cmd) {
    TransportData<CommandBase> transport;
    transport.source = DestinationType::CommandRunner;
    transport.destination = DestinationType::EntanglementRunner;
    transport.node_id = node_id;
    transport.transport = std::move(cmd);
    return broker.route_command(std::move(transport));
}

bool push_sim_command(MessageBroker &broker, uint16_t node_id, std::unique_ptr<CommandBase> cmd) {
    TransportData<CommandBase> transport;
    transport.source = DestinationType::CommandRunner;
    transport.destination = DestinationType::SimRunner;
    transport.node_id = node_id;
    transport.transport = std::move(cmd);
    return broker.route_command(std::move(transport));
}

bool wait_for_response(MessageBroker &broker, TransportData<ResponseData> &out,
                        int max_iters = 200, int sleep_ms = 5) {
    for (int i = 0; i < max_iters; ++i) {
        if (broker.get_response(out)) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
    }
    return false;
}

// Guards BOTH runners' threads: always aborts+joins each in destructor order
// (reverse of construction), even on a mid-test ASSERT_* failure.
struct TwoRunnerGuard {
    TwoRunnerGuard(EntanglementRunner &er, SimulatorRunner &sr)
        : ent_runner(er), sim_runner(sr),
          ent_thread([&er] { er.on_run(-1); }),
          sim_thread([&sr] { sr.on_run(-1); }) {}

    ~TwoRunnerGuard() {
        ent_runner.on_abort();
        sim_runner.on_abort();
        if (ent_thread.joinable()) ent_thread.join();
        if (sim_thread.joinable()) sim_thread.join();
    }

    EntanglementRunner &ent_runner;
    SimulatorRunner &sim_runner;
    std::thread ent_thread;
    std::thread sim_thread;
};

// Guards a single SimulatorRunner's thread (for the ENTANGLEMENT_SWAP test
// below, which never involves EntanglementRunner).
struct SimRunnerGuard {
    explicit SimRunnerGuard(SimulatorRunner &r) : runner(r), thread([&r] { r.on_run(-1); }) {}
    ~SimRunnerGuard() {
        runner.on_abort();
        if (thread.joinable()) thread.join();
    }
    SimulatorRunner &runner;
    std::thread thread;
};

class FullPipelineEntanglementOpsTest : public ::testing::Test {
protected:
    void SetUp() override { broker = std::make_shared<MessageBroker>(); }

    std::unique_ptr<SimulatorRunner> make_sim_runner(int num_nodes) {
        auto sim = std::make_unique<StubSimulator>();
        auto mgr = std::make_unique<SimulatorManager>(std::move(sim));
        for (int i = 0; i < num_nodes; ++i) {
            EXPECT_EQ(mgr->register_node(make_node_config(1)), i);
        }
        return std::make_unique<SimulatorRunner>(std::move(mgr), broker);
    }

    std::shared_ptr<MessageBroker> broker;
};

// A matched ENTANGLEMENT_ATTEMPT pair (nodes 0<->1, permissive short link):
// EntanglementRunner matches both sides and forwards the command to
// SimRunner, and a LIVE SimulatorRunner then actually processes it and routes
// a genuine SUCCESS response back to CommandRunner for BOTH nodes -- the
// cross-runner path neither runner's own unit test exercises end to end.
TEST_F(FullPipelineEntanglementOpsTest, MatchedAttemptProducesRealSuccessResponsesForBothNodes) {
    auto sim_runner = make_sim_runner(/*num_nodes=*/2);
    EntanglementRunner ent_runner(broker, make_permissive_link(0, 1, /*duration_ns=*/100));
    TwoRunnerGuard guard(ent_runner, *sim_runner);

    ASSERT_TRUE(push_entanglement_command(*broker, /*node_id=*/0, make_entanglement_attempt_cmd(/*neighbour=*/1)));
    ASSERT_TRUE(push_entanglement_command(*broker, /*node_id=*/1, make_entanglement_attempt_cmd(/*neighbour=*/0)));

    TransportData<ResponseData> resp1;
    TransportData<ResponseData> resp2;
    ASSERT_TRUE(wait_for_response(*broker, resp1)) << "No response for the first node";
    ASSERT_TRUE(wait_for_response(*broker, resp2)) << "No response for the second node";

    for (const auto *r : {&resp1, &resp2}) {
        EXPECT_EQ(r->source, DestinationType::SimRunner);
        EXPECT_EQ(r->destination, DestinationType::CommandRunner);
        ASSERT_NE(r->transport, nullptr);
        EXPECT_TRUE(r->transport->outcome & static_cast<int>(Outcome_Status_t::SUCCESS));
    }
}

// A request to a neighbour with NO configured attempt model fails fast (per
// EntanglementManager's fail-fast behavior); the resulting ENTANGLEMENT_FAILURE
// response is produced directly by EntanglementRunner (never reaches
// SimRunner), routed to CommandRunner promptly.
TEST_F(FullPipelineEntanglementOpsTest, MissingLinkModelFailsFastEndToEnd) {
    auto sim_runner = make_sim_runner(/*num_nodes=*/2);
    EntanglementRunner ent_runner(broker, AttemptModelsMap{}); // no links configured
    TwoRunnerGuard guard(ent_runner, *sim_runner);

    ASSERT_TRUE(push_entanglement_command(*broker, /*node_id=*/0, make_entanglement_attempt_cmd(/*neighbour=*/1)));
    ASSERT_TRUE(push_entanglement_command(*broker, /*node_id=*/1, make_entanglement_attempt_cmd(/*neighbour=*/0)));

    TransportData<ResponseData> resp1;
    TransportData<ResponseData> resp2;
    ASSERT_TRUE(wait_for_response(*broker, resp1, /*max_iters=*/40, /*sleep_ms=*/1));
    ASSERT_TRUE(wait_for_response(*broker, resp2, /*max_iters=*/40, /*sleep_ms=*/1));

    EXPECT_EQ(resp1.transport->outcome, Outcome_Status_t::ENTANGLEMENT_FAILURE);
    EXPECT_EQ(resp2.transport->outcome, Outcome_Status_t::ENTANGLEMENT_FAILURE);
}

// ENTANGLEMENT_SWAP is dispatched directly to SimRunner (see the
// sim_command_table registration in simulator_manager.cpp) -- it never goes
// through EntanglementRunner/matching at all, unlike ATTEMPT/MEASURE. This
// confirms it still gets a normal end-to-end response through SimulatorRunner
// alone (the TransmissionProcessor-level decode regression for this opcode
// is covered separately in test_transmission_processor_simple.cpp's
// ReceivesEntanglementSwap).
TEST_F(FullPipelineEntanglementOpsTest, EntanglementSwapGetsNormalResponseThroughSimRunnerAlone) {
    auto sim_runner = make_sim_runner(/*num_nodes=*/1);
    SimRunnerGuard guard(*sim_runner);

    ASSERT_TRUE(push_sim_command(*broker, 0, [] {
        uint8_t buf[8] = {0};
        EntangleSwap(/*q1=*/0, /*q2=*/0).create_command(buf, sizeof(buf));
        // NOTE: entanglement_swap_handler validates qubit1_address != qubit2_address
        // is NOT required (it only range-checks, see are_local_qubits_in_range);
        // use the single registered qubit address for both, which is in range.
        return std::make_unique<EntanglementSwap>(buf);
    }()));

    TransportData<ResponseData> response;
    ASSERT_TRUE(wait_for_response(*broker, response));
    ASSERT_NE(response.transport, nullptr);
    EXPECT_TRUE(response.transport->outcome & static_cast<int>(Outcome_Status_t::SUCCESS));
}

} // namespace
