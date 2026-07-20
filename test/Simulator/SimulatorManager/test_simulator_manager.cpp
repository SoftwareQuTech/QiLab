/*
** Tests for SimulatorManager command handling, focused on the per-node
** pre-measurement gate handling and the entanglement Bell-state coordination.
**
** These use a spy ISimulator that records the adapter calls the manager makes,
** so we can assert *which* qubit each basis-rotation lands on and in what order
** relative to establish/measure. This directly guards against the previous
** adapter-global pre-measure latch, which could apply one node's rotation to a
** different node's qubit when commands interleave.
*/

#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "DataTypes/commands.hpp"
#include "DataTypes/responses.hpp"
#include "DataTypes/configuration_options.hpp"
#include "Simulator/sim_interface.hpp"
#include "Simulator/SimulatorManager/simulator_manager.hpp"
#include "Simulator/NoiseModels/qubit_noise_basic.hpp"
#include "mocks/command_mock.hpp"

namespace
{

// ---- Spy adapter -----------------------------------------------------------

struct RecordedOp
{
    enum class Kind
    {
        SingleGate,
        Measure,
        Establish,
        Noise
    };

    Kind kind{};
    uint16_t qubit = 0;      // qubit_address / local_qubit
    Angle_t angle = _PI_16;  // SingleGate only
    Axis_t axis = X_AXIS;    // SingleGate only
    bool basis = false;      // Measure only
    bool is_initiator = false; // Establish only
    uint8_t neighbour = 0;   // Establish only
    float p_dephasing = 0.0f;   // Noise only
    float p_depolarizing = 0.0f; // Noise only
};

class SpyAdapter : public ISimulator
{
public:
    std::vector<RecordedOp> ops;

    // Configurable return values.
    int8_t establish_bell = 0; // Bell state the initiator "identifies".
    uint8_t measure_result = 0;

    bool qubit_initialize(uint8_t) override { return true; }

    bool single_qubit_gate(uint8_t qubit, Angle_t angle, Axis_t axis) override
    {
        RecordedOp op;
        op.kind = RecordedOp::Kind::SingleGate;
        op.qubit = qubit;
        op.angle = angle;
        op.axis = axis;
        ops.push_back(op);
        return true;
    }

    bool two_qubit_gate(uint8_t, uint8_t, Angle_t, Axis_t) override { return true; }

    uint8_t qubit_measure(uint8_t qubit, bool basis) override
    {
        RecordedOp op;
        op.kind = RecordedOp::Kind::Measure;
        op.qubit = qubit;
        op.basis = basis;
        ops.push_back(op);
        return measure_result;
    }

    int8_t attempt_entanglement(uint16_t, uint8_t local_qubit, uint8_t neighbour_address,
                                bool is_initiator, uint8_t) override
    {
        RecordedOp op;
        op.kind = RecordedOp::Kind::Establish;
        op.qubit = local_qubit;
        op.is_initiator = is_initiator;
        op.neighbour = neighbour_address;
        ops.push_back(op);
        // The initiator identifies the Bell state; the responder is a no-op.
        return is_initiator ? establish_bell : static_cast<int8_t>(0);
    }

    bool qubit_move(uint8_t, uint8_t) override { return true; }
    bool qubit_swap(uint8_t, uint8_t) override { return true; }
    bool entanglement_swap(uint8_t, uint8_t) override { return true; }

    bool apply_qubit_noise(uint8_t qubit_address, float p_dephasing, float p_depolarizing) override
    {
        RecordedOp op;
        op.kind = RecordedOp::Kind::Noise;
        op.qubit = qubit_address;
        op.p_dephasing = p_dephasing;
        op.p_depolarizing = p_depolarizing;
        ops.push_back(op);
        return true;
    }

    bool allocate_qubits(uint8_t) override { return true; }
};

// ---- Fixture / helpers -----------------------------------------------------

NodeConfig make_node_config(int num_qubits)
{
    NodeConfig cfg;
    cfg.num_qubits = num_qubits;
    cfg.name = "node";
    cfg.qubit_operations_delays = QuantumOperationsDelaysNs{}; // all-zero delays
    QubitArrayConfig array;
    array.num_qubits = num_qubits;
    array.noise_model.config_data = QubitNoiseBasicConfig{1000.0f};
    cfg.qubit_arrays.push_back(array);
    return cfg;
}

std::unique_ptr<CommandBase> build_command(const BaseC &builder)
{
    uint8_t buf[8] = {0};
    builder.create_command(buf, sizeof(buf));
    switch (static_cast<Command_t>(buf[0]))
    {
    case Command_t::PRE_MEASUREMENT_GATES_SELECTION:
        return std::make_unique<PreMeasureGatesSelection>(buf);
    case Command_t::QUBIT_MEASUREMENT:
        return std::make_unique<QubitMeasure>(buf);
    case Command_t::ENTANGLEMENT_MEASURE:
        return std::make_unique<EntangleMeasure>(buf);
    case Command_t::ENTANGLEMENT_ATTEMPT:
        return std::make_unique<EntanglementAttempt>(buf);
    case Command_t::SINGLE_QUBIT_GATE:
        return std::make_unique<SingleQubitGate>(buf);
    default:
        return nullptr;
    }
}

class SimulatorManagerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        auto spy_owned = std::make_unique<SpyAdapter>();
        spy = spy_owned.get();
        mgr = std::make_unique<SimulatorManager>(std::move(spy_owned));
        // Node 0 -> global qubits {0,1}; Node 1 -> global qubits {2,3}.
        ASSERT_EQ(mgr->register_node(make_node_config(2)), 0);
        ASSERT_EQ(mgr->register_node(make_node_config(2)), 1);
    }

    ResponseData run(uint16_t node_id, std::unique_ptr<CommandBase> cmd)
    {
        ResponseData response;
        EXPECT_EQ(mgr->add_new_command_to_process(node_id, std::move(cmd), response), 0);
        return response;
    }

    SpyAdapter *spy = nullptr; // owned by mgr
    std::unique_ptr<SimulatorManager> mgr;
};

// Same setup as SimulatorManagerTest, but qubit noise is disabled at construction.
class SimulatorManagerNoiseDisabledTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        auto spy_owned = std::make_unique<SpyAdapter>();
        spy = spy_owned.get();
        mgr = std::make_unique<SimulatorManager>(std::move(spy_owned), /*qubit_noise_enabled=*/false);
        ASSERT_EQ(mgr->register_node(make_node_config(2)), 0);
    }

    ResponseData run(uint16_t node_id, std::unique_ptr<CommandBase> cmd)
    {
        ResponseData response;
        EXPECT_EQ(mgr->add_new_command_to_process(node_id, std::move(cmd), response), 0);
        return response;
    }

    SpyAdapter *spy = nullptr; // owned by mgr
    std::unique_ptr<SimulatorManager> mgr;
};

// The neighbour field is encoded 1-based on the wire and decoded to 0-based, so
// selecting neighbour node `id` requires passing `id + 1` to the builder.
constexpr uint8_t wire_nghbr(uint8_t node_id) { return static_cast<uint8_t>(node_id + 1); }

// ---- Tests -----------------------------------------------------------------

// A regular measurement applies the node's selected basis rotation (X-Y-X) to
// that node's own global qubit, then measures the same qubit.
TEST_F(SimulatorManagerTest, RegularMeasureRotatesOwnQubitThenMeasures)
{
    auto sel = build_command(PreMeasureGateSelect(_2_PI_16, _3_PI_16, _4_PI_16, /*target=*/false));
    auto *sel_ptr = static_cast<PreMeasureGatesSelection *>(sel.get());
    const Angle_t a1 = sel_ptr->angle1, a2 = sel_ptr->angle2, a3 = sel_ptr->angle3;
    run(/*node=*/0, std::move(sel));
    EXPECT_TRUE(spy->ops.empty()) << "selection must not touch the simulator";

    // Node 0, local qubit 1 -> global qubit 1.
    run(/*node=*/0, build_command(QMeasure(/*addr=*/1, /*basis=*/false, X_AXIS)));

    ASSERT_EQ(spy->ops.size(), 4u);
    EXPECT_EQ(spy->ops[0].kind, RecordedOp::Kind::SingleGate);
    EXPECT_EQ(spy->ops[0].qubit, 1u);
    EXPECT_EQ(spy->ops[0].axis, X_AXIS);
    EXPECT_EQ(spy->ops[0].angle, a1);
    EXPECT_EQ(spy->ops[1].axis, Y_AXIS);
    EXPECT_EQ(spy->ops[1].qubit, 1u);
    EXPECT_EQ(spy->ops[1].angle, a2);
    EXPECT_EQ(spy->ops[2].axis, X_AXIS);
    EXPECT_EQ(spy->ops[2].qubit, 1u);
    EXPECT_EQ(spy->ops[2].angle, a3);
    EXPECT_EQ(spy->ops[3].kind, RecordedOp::Kind::Measure);
    EXPECT_EQ(spy->ops[3].qubit, 1u);
}

// The core cross-node regression: node 0 selects pre-measure gates, but node 1
// measures first. Node 1's measurement must NOT pick up node 0's rotation, and
// node 0's later measurement must still get it.
TEST_F(SimulatorManagerTest, SelectionIsNotConsumedByAnotherNode)
{
    run(/*node=*/0, build_command(PreMeasureGateSelect(_2_PI_16, _3_PI_16, _4_PI_16, /*target=*/false)));

    // Node 1 measures first (local qubit 0 -> global qubit 2): no rotation.
    run(/*node=*/1, build_command(QMeasure(/*addr=*/0, /*basis=*/false, X_AXIS)));
    ASSERT_EQ(spy->ops.size(), 1u);
    EXPECT_EQ(spy->ops[0].kind, RecordedOp::Kind::Measure);
    EXPECT_EQ(spy->ops[0].qubit, 2u);

    // Node 0 measures (local qubit 0 -> global qubit 0): rotation still applied.
    run(/*node=*/0, build_command(QMeasure(/*addr=*/0, /*basis=*/false, X_AXIS)));
    ASSERT_EQ(spy->ops.size(), 5u);
    EXPECT_EQ(spy->ops[1].kind, RecordedOp::Kind::SingleGate);
    EXPECT_EQ(spy->ops[1].qubit, 0u);
    EXPECT_EQ(spy->ops[2].qubit, 0u);
    EXPECT_EQ(spy->ops[3].qubit, 0u);
    EXPECT_EQ(spy->ops[4].kind, RecordedOp::Kind::Measure);
    EXPECT_EQ(spy->ops[4].qubit, 0u);
}

// A selection is one-shot: consumed by the first matching measurement only.
TEST_F(SimulatorManagerTest, SelectionAppliesOnlyOnce)
{
    run(/*node=*/0, build_command(PreMeasureGateSelect(_2_PI_16, _3_PI_16, _4_PI_16, /*target=*/false)));

    run(/*node=*/0, build_command(QMeasure(/*addr=*/0, /*basis=*/false, X_AXIS)));
    ASSERT_EQ(spy->ops.size(), 4u); // 3 rotations + measure

    run(/*node=*/0, build_command(QMeasure(/*addr=*/0, /*basis=*/false, X_AXIS)));
    ASSERT_EQ(spy->ops.size(), 5u); // only a measure this time, no new rotations
    EXPECT_EQ(spy->ops[4].kind, RecordedOp::Kind::Measure);
}

// The entanglement measure path: establish (identify Bell state) must come
// first, THEN the basis rotation on the node's own qubit, THEN the measurement.
TEST_F(SimulatorManagerTest, EntanglementMeasureEstablishesThenRotatesThenMeasures)
{
    // target=1 selects the entanglement-measure slot.
    run(/*node=*/0, build_command(PreMeasureGateSelect(_2_PI_16, _3_PI_16, _4_PI_16, /*target=*/true)));

    // Node 0 measures entanglement with node 1 (initiator, since 0 < 1).
    run(/*node=*/0, build_command(EntanMeasure(/*fidelity=*/0, wire_nghbr(1), /*qid=*/0,
                                               /*try_now=*/true, /*basis=*/false, /*repeat=*/0)));

    ASSERT_EQ(spy->ops.size(), 5u);
    EXPECT_EQ(spy->ops[0].kind, RecordedOp::Kind::Establish);
    EXPECT_EQ(spy->ops[0].qubit, 0u);           // node 0 local qubit -> global 0
    EXPECT_TRUE(spy->ops[0].is_initiator);
    EXPECT_EQ(spy->ops[0].neighbour, 2u);       // node 1's global qubit offset
    EXPECT_EQ(spy->ops[1].kind, RecordedOp::Kind::SingleGate);
    EXPECT_EQ(spy->ops[1].qubit, 0u);
    EXPECT_EQ(spy->ops[2].qubit, 0u);
    EXPECT_EQ(spy->ops[3].qubit, 0u);
    EXPECT_EQ(spy->ops[4].kind, RecordedOp::Kind::Measure);
    EXPECT_EQ(spy->ops[4].qubit, 0u);
}

// Both nodes of a matched pair report the SAME 4-way Bell state: the initiator
// identifies it and caches it; the responder reads it back from the manager.
TEST_F(SimulatorManagerTest, BothNodesReportSameBellState)
{
    spy->establish_bell = STATE_PSI_PLUS; // 2

    // Initiator (node 0 < node 1) establishes and caches the Bell state.
    ResponseData r0 = run(/*node=*/0, build_command(EntanMeasure(0, wire_nghbr(1), 0, true, false, 0)));
    // Responder (node 1) reads the cached Bell state.
    ResponseData r1 = run(/*node=*/1, build_command(EntanMeasure(0, wire_nghbr(0), 0, true, false, 0)));

    EXPECT_EQ(r0.outcome & 0x03, static_cast<int>(STATE_PSI_PLUS));
    EXPECT_EQ(r1.outcome & 0x03, static_cast<int>(STATE_PSI_PLUS));
    EXPECT_TRUE(r0.outcome & static_cast<int>(Outcome_Status_t::SUCCESS));
    EXPECT_TRUE(r1.outcome & static_cast<int>(Outcome_Status_t::SUCCESS));
}

// The measured outcome bit (bit 2) reflects this node's own measurement result.
TEST_F(SimulatorManagerTest, EntanglementMeasureEncodesOwnOutcomeBit)
{
    spy->establish_bell = STATE_PHI_PLUS; // 0
    spy->measure_result = 1;              // force outcome bit set

    ResponseData r = run(/*node=*/0, build_command(EntanMeasure(0, wire_nghbr(1), 0, true, false, 0)));
    EXPECT_EQ(r.outcome & 0x04, 0x04);
    EXPECT_EQ(r.outcome & 0x03, static_cast<int>(STATE_PHI_PLUS));
}

// ---- Qubit-in-memory noise --------------------------------------------------

// A single-qubit gate with a nonzero angle applies qubit-in-memory (dephasing)
// noise to the target qubit before the gate itself, when noise is enabled.
TEST_F(SimulatorManagerTest, SingleQubitGateAppliesNoiseWhenEnabled)
{
    run(/*node=*/0, build_command(SingleQGate(/*addr=*/1, _2_PI_16, X_AXIS)));

    ASSERT_EQ(spy->ops.size(), 2u);
    EXPECT_EQ(spy->ops[0].kind, RecordedOp::Kind::Noise);
    EXPECT_EQ(spy->ops[0].qubit, 1u);
    EXPECT_FLOAT_EQ(spy->ops[0].p_depolarizing, 0.0f);
    EXPECT_EQ(spy->ops[1].kind, RecordedOp::Kind::SingleGate);
    EXPECT_EQ(spy->ops[1].qubit, 1u);
}

// The angle==0 early-return path performs no gate and no noise.
TEST_F(SimulatorManagerTest, SingleQubitGateWithZeroAngleAppliesNoNoise)
{
    run(/*node=*/0, build_command(SingleQGate(/*addr=*/1, _0_PI_16, X_AXIS)));
    EXPECT_TRUE(spy->ops.empty());
}

// When qubit_noise_enabled is false, no noise is applied for single-qubit gates.
TEST_F(SimulatorManagerNoiseDisabledTest, SingleQubitGateAppliesNoNoiseWhenDisabled)
{
    run(/*node=*/0, build_command(SingleQGate(/*addr=*/1, _2_PI_16, X_AXIS)));

    ASSERT_EQ(spy->ops.size(), 1u);
    EXPECT_EQ(spy->ops[0].kind, RecordedOp::Kind::SingleGate);
    EXPECT_EQ(spy->ops[0].qubit, 1u);
}

} // namespace
