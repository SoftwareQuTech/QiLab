#include <gtest/gtest.h>

#include <memory>
#include <random>

#include "DataTypes/commands.hpp"
#include "Simulator/SimulatorAdapters/stim_nv_adapter.hpp"

// Unit tests for StimAdapterNVCenters (src/Simulator/SimulatorAdapters/stim_nv_adapter.cpp).
//
// StimAdapterNVCenters wraps a real stim::TableauSimulator, so simple circuits have
// quantum-mechanically deterministic, checkable outcomes. Where the tests below assert
// exact outcomes, the surrounding comment explains the stabilizer-level reasoning; where
// the adapter's own behavior is genuinely probabilistic (or was found to differ from what
// its doc comments suggest), that is called out explicitly rather than asserted as exact.
//
// IMPORTANT, actual-behavior findings from reading the .cpp (not just the .hpp docstrings):
//  * qubit_measure() ALWAYS performs a plain Z-basis measurement internally. The `basis`
//    parameter is only used for a log message; no basis-rotation is applied inside this
//    method. Per the method's own comment, basis rotations for X-basis reads are expected
//    to be applied by the caller (SimulatorManager) via single_qubit_gate BEFORE calling
//    qubit_measure. Tests that want a deterministic "X-basis" outcome therefore apply the
//    rotate-then-Z-measure protocol manually, exactly like SimulatorManager would.
//  * two_qubit_gate's angle=_MINUS_PI branch is NOT a controlled/entangling gate at all:
//    for axis=X_AXIS it applies a plain Z gate to qubit1 and a plain X gate to qubit2,
//    completely independently of one another (a product operator, mathematically
//    equal, up to global phase, to the theta=pi point of an RZX(theta) rotation). It does
//    NOT implement a conditional "flip target iff control is 1" CNOT semantics.
//  * two_qubit_gate's angle=_8_PI_16 (axis=X_AXIS) branch is the one real entangling
//    circuit (ZCX / CNOT plus SQRT_Z on qubit1 and SQRT_X on qubit2 corrections). Working
//    through the stabilizer propagation: SQRT_Z (S) commutes exactly with Z, so it never
//    touches the control qubit's Z operator, and SQRT_X only touches qubit2 - so the
//    control qubit's Z-eigenvalue survives the whole sequence unchanged and is exactly
//    preserved/deterministic. But SQRT_X maps Z_target -> +-Y_target (a sign-convention
//    detail, but the axis change itself is not), so a plain Z-measurement of qubit2 right
//    after this gate is NOT deterministic (~50/50) - this is not a "textbook CNOT" that
//    cleanly flips the target into a computational basis state.
//  * attempt_entanglement's docstring calls the return value an "implementation-defined
//    status byte", but the concrete implementation always drives the same fixed circuit
//    for the initiator (R,R,H,X,CX) and always lands on the |Psi+> Bell pair, so the
//    initiator branch deterministically returns 2 (not just "some Bell index").
//  * qubit_move performs a real SWAP followed by re-initializing (RZ) the source address,
//    i.e. it really moves state from qubit1 into qubit2 and resets qubit1 to |0>.
//  * qubit_swap performs a real SWAP with no reset afterwards (unlike qubit_move).
//  * entanglement_swap is an unimplemented stub in this adapter too (like the plain
//    stim_adapter.cpp sibling) - the whole body is commented out; it just logs an error
//    and unconditionally returns false, even without a self-address check.

namespace {

class StimAdapterNVCentersTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        seed = std::make_unique<std::seed_seq>(std::initializer_list<int>{1, 2, 3, 4});
        adapter = std::make_unique<StimAdapterNVCenters>(*seed, /*num_qubits=*/20);
    }

    std::unique_ptr<std::seed_seq> seed;
    std::unique_ptr<StimAdapterNVCenters> adapter;
};

// ---------------------------------------------------------------------------
// 1. Fresh-qubit initialization + Z-basis measurement.
// ---------------------------------------------------------------------------

TEST_F(StimAdapterNVCentersTest, FreshQubitMeasuresZeroInZBasis)
{
    for (const uint8_t address : {0, 1, 5, 10, 19})
    {
        EXPECT_TRUE(adapter->qubit_initialize(address));
        EXPECT_EQ(adapter->qubit_measure(address, /*basis=Z*/ false), 0)
            << "qubit " << static_cast<int>(address) << " should read |0> right after init";
    }
}

// ---------------------------------------------------------------------------
// 2. X gate then Z-basis measurement: deterministic bit flip.
// ---------------------------------------------------------------------------

TEST_F(StimAdapterNVCentersTest, XGateFlipsZeroToOne)
{
    const uint8_t address = 3;
    ASSERT_TRUE(adapter->qubit_initialize(address));
    EXPECT_TRUE(adapter->single_qubit_gate(address, Angle_t::_MINUS_PI, Axis_t::X_AXIS));
    EXPECT_EQ(adapter->qubit_measure(address, false), 1);
}

TEST_F(StimAdapterNVCentersTest, ZGateOnZeroLeavesComputationalStateUnchanged)
{
    // Z|0> = +|0> (only a phase, no measurable bit flip in the Z basis).
    const uint8_t address = 4;
    ASSERT_TRUE(adapter->qubit_initialize(address));
    EXPECT_TRUE(adapter->single_qubit_gate(address, Angle_t::_MINUS_PI, Axis_t::Z_AXIS));
    EXPECT_EQ(adapter->qubit_measure(address, false), 0);
}

// ---------------------------------------------------------------------------
// 3. SQRT_Y (H-equivalent for |0>) + rotate-back-then-Z-measure emulates a
//    deterministic X-basis read, since qubit_measure never rotates internally.
// ---------------------------------------------------------------------------

TEST_F(StimAdapterNVCentersTest, RotateToPlusAndBackGivesDeterministicMeasurement)
{
    // SQRT_Y = exp(-i*pi/4*Y) maps |0> -> |+> exactly (real amplitudes, no phase
    // subtlety), i.e. it plays the role H would play, but single_qubit_gate never
    // exposes a raw H mapping - only X/Y/Z (angle=pi) and SQRT_X/Y/Z (angle=pi/2).
    // Applying SQRT_Y then its dagger is the identity, so the qubit is deterministically
    // back at |0>. This exercises the same "rotate, then plain Z-measure" protocol that
    // SimulatorManager uses for X-basis reads (see qubit_measure's own comment: it never
    // rotates internally regardless of the `basis` argument).
    const uint8_t address = 6;
    ASSERT_TRUE(adapter->qubit_initialize(address));
    EXPECT_TRUE(adapter->single_qubit_gate(address, Angle_t::_8_PI_16, Axis_t::Y_AXIS));         // SQRT_Y: |0> -> |+>
    EXPECT_TRUE(adapter->single_qubit_gate(address, Angle_t::_MINUS_8_PI_16, Axis_t::Y_AXIS));   // SQRT_Y_DAG: |+> -> |0>
    EXPECT_EQ(adapter->qubit_measure(address, /*basis label=X*/ true), 0);
}

TEST_F(StimAdapterNVCentersTest, DirectZMeasurementOfPlusStateIsNotDeterministic)
{
    // Soft/statistical sanity check only (optional per task spec): a fresh adapter is
    // rotated to |+> and then Z-measured directly with NO basis rotation applied first
    // (this is the "wrong" thing to do, quantum-mechanically, but it is exactly what
    // qubit_measure(addr, true) actually does internally - it never rotates). Both
    // outcomes must be reachable across trials with different seeds; we do not assert
    // any single exact bit.
    bool saw_zero = false;
    bool saw_one = false;
    for (int trial = 0; trial < 100 && !(saw_zero && saw_one); ++trial)
    {
        std::seed_seq trial_seed{trial * 7 + 1, trial * 13 + 2, trial * 29 + 3};
        StimAdapterNVCenters trial_adapter(trial_seed, /*num_qubits=*/4);
        ASSERT_TRUE(trial_adapter.qubit_initialize(0));
        ASSERT_TRUE(trial_adapter.single_qubit_gate(0, Angle_t::_8_PI_16, Axis_t::Y_AXIS)); // -> |+>
        const uint8_t result = trial_adapter.qubit_measure(0, false);
        if (result == 0)
            saw_zero = true;
        else
            saw_one = true;
    }
    EXPECT_TRUE(saw_zero) << "never observed a 0 outcome across 100 trials";
    EXPECT_TRUE(saw_one) << "never observed a 1 outcome across 100 trials";
}

// ---------------------------------------------------------------------------
// 4. Zero-angle no-op for single_qubit_gate and two_qubit_gate.
// ---------------------------------------------------------------------------

// The zero-angle no-op decision now lives solely in SimulatorManager (which
// never forwards an Angle_t::_0_PI_16 gate to the adapter at all -- see
// SimulatorManager::single_qubit_gate_handler/two_qubit_gate_handler), so the
// adapter-level no-op branches were removed as dead code. Called directly with
// a zero angle (bypassing the manager), the adapter now falls through to its
// "unsupported angle" branch like any other unhandled angle: it returns
// false and leaves the state untouched (matching StimAdapter's pre-existing
// behavior for angle=0, which never had a special-case branch either).
TEST_F(StimAdapterNVCentersTest, SingleQubitGateZeroAngleIsUnsupported)
{
    const uint8_t address = 7;
    ASSERT_TRUE(adapter->qubit_initialize(address));
    ASSERT_TRUE(adapter->single_qubit_gate(address, Angle_t::_MINUS_PI, Axis_t::X_AXIS)); // flip to |1>
    ASSERT_EQ(adapter->qubit_measure(address, false), 1);

    // Re-initialize to |1> is destroyed by measurement collapse; redo the flip so we can
    // check the zero-angle gate really does nothing to a known non-zero state.
    ASSERT_TRUE(adapter->qubit_initialize(address));
    ASSERT_TRUE(adapter->single_qubit_gate(address, Angle_t::_MINUS_PI, Axis_t::X_AXIS)); // |1> again
    EXPECT_FALSE(adapter->single_qubit_gate(address, Angle_t::_0_PI_16, Axis_t::X_AXIS)); // unsupported, returns false
    EXPECT_EQ(adapter->qubit_measure(address, false), 1) << "unsupported zero-angle gate must not alter the state";
}

TEST_F(StimAdapterNVCentersTest, TwoQubitGateZeroAngleIsUnsupported)
{
    const uint8_t q1 = 8;
    const uint8_t q2 = 9;
    ASSERT_TRUE(adapter->qubit_initialize(q1));
    ASSERT_TRUE(adapter->qubit_initialize(q2));
    ASSERT_TRUE(adapter->single_qubit_gate(q2, Angle_t::_MINUS_PI, Axis_t::X_AXIS)); // q2 -> |1>, q1 stays |0>

    EXPECT_FALSE(adapter->two_qubit_gate(q1, q2, Angle_t::_0_PI_16, Axis_t::X_AXIS));

    EXPECT_EQ(adapter->qubit_measure(q1, false), 0);
    EXPECT_EQ(adapter->qubit_measure(q2, false), 1);
}

// ---------------------------------------------------------------------------
// 5. two_qubit_gate rejects qubit1_address == qubit2_address.
// ---------------------------------------------------------------------------

TEST_F(StimAdapterNVCentersTest, TwoQubitGateRejectsSameAddressRegardlessOfAngle)
{
    const uint8_t address = 2;
    ASSERT_TRUE(adapter->qubit_initialize(address));
    EXPECT_FALSE(adapter->two_qubit_gate(address, address, Angle_t::_MINUS_PI, Axis_t::X_AXIS));
    // The same-address check runs before the angle dispatch, so even the "no-op" angle
    // is still rejected for identical addresses.
    EXPECT_FALSE(adapter->two_qubit_gate(address, address, Angle_t::_0_PI_16, Axis_t::X_AXIS));
}

// ---------------------------------------------------------------------------
// 6. Concrete two-qubit gates: actual (not textbook-CNOT-assumed) behavior.
// ---------------------------------------------------------------------------

TEST_F(StimAdapterNVCentersTest, TwoQubitGateMinusPiAppliesIndependentZXProductNotAControlledFlip)
{
    // angle=_MINUS_PI, axis=X_AXIS applies a plain Z to qubit1 and a plain X to qubit2,
    // unconditionally - it is NOT a controlled gate. Demonstrate this by showing the
    // target always flips regardless of the "control" qubit's value.

    // Case A: control(q1)=|0>
    {
        const uint8_t q1 = 10;
        const uint8_t q2 = 11;
        ASSERT_TRUE(adapter->qubit_initialize(q1));
        ASSERT_TRUE(adapter->qubit_initialize(q2));
        EXPECT_TRUE(adapter->two_qubit_gate(q1, q2, Angle_t::_MINUS_PI, Axis_t::X_AXIS));
        EXPECT_EQ(adapter->qubit_measure(q1, false), 0) << "Z on |0> is a phase only, no bit flip";
        EXPECT_EQ(adapter->qubit_measure(q2, false), 1) << "X unconditionally flips qubit2";
    }

    // Case B: control(q1)=|1> - target still flips, and q1's classical value is
    // unaffected (Z again only contributes a phase), confirming this is a product
    // operator rather than a conditional flip.
    {
        const uint8_t q1 = 12;
        const uint8_t q2 = 13;
        ASSERT_TRUE(adapter->qubit_initialize(q1));
        ASSERT_TRUE(adapter->qubit_initialize(q2));
        ASSERT_TRUE(adapter->single_qubit_gate(q1, Angle_t::_MINUS_PI, Axis_t::X_AXIS)); // q1 -> |1>
        EXPECT_TRUE(adapter->two_qubit_gate(q1, q2, Angle_t::_MINUS_PI, Axis_t::X_AXIS));
        EXPECT_EQ(adapter->qubit_measure(q1, false), 1) << "Z preserves the |1> computational value";
        EXPECT_EQ(adapter->qubit_measure(q2, false), 1) << "X unconditionally flips qubit2 either way";
    }
}

TEST_F(StimAdapterNVCentersTest, TwoQubitGate8Pi16PreservesControlQubitDeterministically)
{
    // angle=_8_PI_16, axis=X_AXIS applies ZCX(q1=control,q2=target) then SQRT_Z(q1),
    // SQRT_X(q2). Stabilizer tracking: starting from control=|1> (stabilizer -Z_q1) and
    // target=|0> (stabilizer +Z_q2), CX turns this into stabilizers {-Z_q1, +Z_q1 Z_q2}.
    // SQRT_Z commutes exactly with Z, so it never changes the -Z_q1 generator; SQRT_X
    // only acts on q2, so it cannot touch a generator that is pure Z_q1 either. The
    // control qubit's Z-value is therefore exactly preserved through the whole gate.
    //
    // (SQRT_X does rotate the Z_q2 factor in the *other* generator into +-Y_q2, so a
    // direct Z-measurement of q2 right after this gate is NOT deterministic - this is
    // intentionally not asserted here; this is not a plain textbook CNOT.)
    const uint8_t q1 = 14; // control
    const uint8_t q2 = 15; // target
    ASSERT_TRUE(adapter->qubit_initialize(q1));
    ASSERT_TRUE(adapter->qubit_initialize(q2));
    ASSERT_TRUE(adapter->single_qubit_gate(q1, Angle_t::_MINUS_PI, Axis_t::X_AXIS)); // q1 -> |1>

    EXPECT_TRUE(adapter->two_qubit_gate(q1, q2, Angle_t::_8_PI_16, Axis_t::X_AXIS));

    EXPECT_EQ(adapter->qubit_measure(q1, false), 1)
        << "control qubit's Z-eigenvalue must survive CX+SQRT_Z+SQRT_X exactly";
}

// ---------------------------------------------------------------------------
// 7. allocate_qubits boundary at MAX_QUBITS_AVAILABLE (250).
// ---------------------------------------------------------------------------

TEST(StimAdapterNVCentersAllocateTest, SucceedsExactlyUpToMax)
{
    std::seed_seq seed{42};
    StimAdapterNVCenters local_adapter(seed, /*num_qubits=*/8);

    EXPECT_TRUE(local_adapter.allocate_qubits(242)); // 8 + 242 == 250, exactly at the bound
    // Registered qubits is now exactly 250; one more must be rejected.
    EXPECT_FALSE(local_adapter.allocate_qubits(1));
}

TEST(StimAdapterNVCentersAllocateTest, FailsWhenExceedingMax)
{
    std::seed_seq seed{43};
    StimAdapterNVCenters local_adapter(seed, /*num_qubits=*/8);

    EXPECT_FALSE(local_adapter.allocate_qubits(243)); // 8 + 243 == 251 > 250
}

TEST(StimAdapterNVCentersAllocateTest, NewlyAllocatedRangeIsUsable)
{
    // Indirect observation of the registered_qubits bookkeeping: after allocating more
    // qubits, an address inside the newly-extended range must be usable (Stim's tableau
    // was actually resized via ensure_large_enough_for_qubits).
    std::seed_seq seed{44};
    StimAdapterNVCenters local_adapter(seed, /*num_qubits=*/8);

    ASSERT_TRUE(local_adapter.allocate_qubits(10)); // registered_qubits: 8 -> 18

    const uint8_t new_address = 15; // inside [8, 18), unusable before the allocation
    EXPECT_TRUE(local_adapter.qubit_initialize(new_address));
    EXPECT_EQ(local_adapter.qubit_measure(new_address, false), 0);
}

// ---------------------------------------------------------------------------
// 8. attempt_entanglement.
// ---------------------------------------------------------------------------

TEST_F(StimAdapterNVCentersTest, AttemptEntanglementRejectsSelfEntanglement)
{
    EXPECT_EQ(adapter->attempt_entanglement(/*fidelity=*/50, /*local_qubit=*/3,
                                             /*neighbour_address=*/3, /*is_initiator=*/true, /*qid=*/0),
              -1);
    EXPECT_EQ(adapter->attempt_entanglement(50, 3, 3, /*is_initiator=*/false, 0), -1);
}

TEST_F(StimAdapterNVCentersTest, AttemptEntanglementResponderReturnsZeroWithoutTouchingSimulator)
{
    // Actual behavior (not the header's generic "mock" docstring): when is_initiator is
    // false, the method does nothing to the tableau at all and unconditionally returns 0
    // - the Bell state is assumed to already be supplied by the SimulatorManager cache.
    EXPECT_EQ(adapter->attempt_entanglement(50, 1, 2, /*is_initiator=*/false, 0), 0);
}

TEST_F(StimAdapterNVCentersTest, AttemptEntanglementInitiatorDeterministicallyProducesPsiPlus)
{
    // Actual behavior: the initiator branch always drives the exact same fixed circuit
    // (R, R, H on local, X on neighbour, CX local->neighbour), which deterministically
    // prepares |Psi+> = (|01>+|10>)/sqrt2 regardless of RNG seed. Following the code's own
    // XX/ZZ observable convention (XX=+1,ZZ=+1 -> 0 |Phi+>; XX=-1,ZZ=+1 -> 1 |Phi->;
    // XX=+1,ZZ=-1 -> 2 |Psi+>; XX=-1,ZZ=-1 -> 3 |Psi->): |Psi+> has XX=+1 and ZZ=-1, so
    // the method must return 2, every single time - this is fully deterministic, not a
    // "mock status byte" as the header comment implies.
    EXPECT_EQ(adapter->attempt_entanglement(/*fidelity=*/50, /*local_qubit=*/4,
                                             /*neighbour_address=*/5, /*is_initiator=*/true, /*qid=*/0),
              2);
}

// ---------------------------------------------------------------------------
// 9. qubit_move / qubit_swap / entanglement_swap actual behavior.
// ---------------------------------------------------------------------------

TEST_F(StimAdapterNVCentersTest, QubitMoveRejectsSameAddress)
{
    EXPECT_FALSE(adapter->qubit_move(3, 3));
}

TEST_F(StimAdapterNVCentersTest, QubitMoveTransfersStateAndResetsSource)
{
    // qubit_move does a real SWAP(q1,q2) and then re-initializes (RZ) q1: the net effect
    // is that q1's original state ends up at q2, and q1 itself is forced back to |0>.
    const uint8_t q1 = 16;
    const uint8_t q2 = 17;
    ASSERT_TRUE(adapter->qubit_initialize(q1));
    ASSERT_TRUE(adapter->qubit_initialize(q2));
    ASSERT_TRUE(adapter->single_qubit_gate(q1, Angle_t::_MINUS_PI, Axis_t::X_AXIS)); // q1 -> |1>

    EXPECT_TRUE(adapter->qubit_move(q1, q2));

    EXPECT_EQ(adapter->qubit_measure(q1, false), 0) << "source is force-reset to |0> after the move";
    EXPECT_EQ(adapter->qubit_measure(q2, false), 1) << "destination now holds the moved-in |1>";
}

TEST_F(StimAdapterNVCentersTest, QubitSwapRejectsSameAddress)
{
    EXPECT_FALSE(adapter->qubit_swap(3, 3));
}

TEST_F(StimAdapterNVCentersTest, QubitSwapExchangesStatesWithoutResettingSource)
{
    // qubit_swap performs a genuine SWAP gate with no re-initialization afterwards
    // (unlike qubit_move).
    const uint8_t q1 = 18;
    const uint8_t q2 = 19;
    ASSERT_TRUE(adapter->qubit_initialize(q1));
    ASSERT_TRUE(adapter->qubit_initialize(q2));
    ASSERT_TRUE(adapter->single_qubit_gate(q1, Angle_t::_MINUS_PI, Axis_t::X_AXIS)); // q1 -> |1>

    EXPECT_TRUE(adapter->qubit_swap(q1, q2));

    EXPECT_EQ(adapter->qubit_measure(q1, false), 0) << "q1 now holds q2's original |0>";
    EXPECT_EQ(adapter->qubit_measure(q2, false), 1) << "q2 now holds q1's original |1>";
}

TEST_F(StimAdapterNVCentersTest, EntanglementSwapIsAnUnimplementedStub)
{
    // Actual behavior: like the sibling stim_adapter.cpp, entanglement_swap here is not
    // implemented - the real gate sequence is commented out in the source, and the
    // function unconditionally logs an error and returns false. There isn't even a
    // same-address validation (unlike qubit_move/qubit_swap), because no logic runs at
    // all besides the log + return.
    EXPECT_FALSE(adapter->entanglement_swap(0, 1));
    EXPECT_FALSE(adapter->entanglement_swap(2, 2));
}

// ---------------------------------------------------------------------------
// single_qubit_noise: deterministic identity case only (nonzero probabilities are
// inherently probabilistic and are intentionally not asserted to an exact post-state).
// ---------------------------------------------------------------------------

TEST_F(StimAdapterNVCentersTest, SingleQubitNoiseWithZeroProbabilitiesIsIdentity)
{
    const uint8_t address = 1;
    ASSERT_TRUE(adapter->qubit_initialize(address));
    EXPECT_TRUE(adapter->single_qubit_noise(address, 0.0f, 0.0f, 0.0f));
    EXPECT_EQ(adapter->qubit_measure(address, false), 0);
}

TEST_F(StimAdapterNVCentersTest, SingleQubitNoiseWithZeroProbabilitiesLeavesOneStateUnchanged)
{
    const uint8_t address = 5;
    ASSERT_TRUE(adapter->qubit_initialize(address));
    ASSERT_TRUE(adapter->single_qubit_gate(address, Angle_t::_MINUS_PI, Axis_t::X_AXIS)); // -> |1>
    EXPECT_TRUE(adapter->single_qubit_noise(address, 0.0f, 0.0f, 0.0f));
    EXPECT_EQ(adapter->qubit_measure(address, false), 1);
}

} // namespace
