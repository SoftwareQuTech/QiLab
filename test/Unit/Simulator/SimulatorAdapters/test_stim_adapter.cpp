// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <memory>
#include <random>

#include <gtest/gtest.h>

#include "DataTypes/commands.hpp"
#include "Simulator/SimulatorAdapters/stim_adapter.hpp"

// Unit tests for StimAdapter (src/Simulator/SimulatorAdapters/stim_adapter.cpp/.hpp),
// the plain (non NV-centers) Stim-backed ISimulator implementation. StimAdapter
// wraps a *real* stim::TableauSimulator -- not a mock -- so outcomes below are
// real, quantum-mechanically-determined results for these (stabilizer) circuits,
// not canned/hardcoded return values.
//
// Notable divergences from the sibling NV-centers adapter (stim_nv_adapter.cpp),
// confirmed by reading this file's actual implementation:
//  - single_qubit_gate/two_qubit_gate have NO Angle_t::_0_PI_16 no-op special
//    case here. angle=0 always falls through to the final "unsupported angle"
//    branch and returns false without touching simulator state.
//  - qubit_move and entanglement_swap are unconditional stubs: they log an
//    error ("... is not implemented yet!") and return false regardless of
//    arguments; entanglement_swap's body is entirely commented out.
//  - single_qubit_gate exposes X/Y/Z and SQRT_X/SQRT_X_DAG/SQRT_Y/SQRT_Y_DAG/
//    SQRT_Z/SQRT_Z_DAG only -- there is no (angle, axis) combination that maps
//    to Stim's H gate. H is only ever applied internally, inside
//    attempt_entanglement, and is not reachable through the public interface.
//  - two_qubit_gate exposes SQRT_XX/SQRT_XX_DAG (X_AXIS) and SQRT_YY/SQRT_YY_DAG
//    (Y_AXIS) as its genuine two-qubit gates; its Angle_t::_MINUS_PI branch is
//    NOT a two-qubit gate at all -- it just applies a plain single-qubit X (or
//    Y) gate to each of the two qubits independently. There is no CX/CZ mapping
//    anywhere in this file. Z_AXIS is entirely unsupported for two_qubit_gate.

namespace {

// Builds a fresh, reproducibly-seeded StimAdapter with `num_qubits` pre-allocated.
// StimAdapter is non-copyable/non-movable, so it's returned via unique_ptr.
std::unique_ptr<StimAdapter> make_adapter(uint8_t num_qubits)
{
    std::seed_seq seed{1, 2, 3, 4, 5};
    return std::make_unique<StimAdapter>(seed, num_qubits);
}

} // namespace

// ---- qubit_initialize / qubit_measure (Z-basis) ----

TEST(StimAdapterTest, FreshlyInitializedQubitsMeasureZeroInZBasis)
{
    auto adapter = make_adapter(8);
    for (uint8_t q : {0, 3, 7})
    {
        ASSERT_TRUE(adapter->qubit_initialize(q));
        EXPECT_EQ(adapter->qubit_measure(q, false), 0) << "qubit " << +q;
    }
}

// ---- single_qubit_gate ----

TEST(StimAdapterTest, XGateFlipsQubitToOneDeterministically)
{
    auto adapter = make_adapter(4);
    ASSERT_TRUE(adapter->qubit_initialize(0));
    ASSERT_TRUE(adapter->single_qubit_gate(0, Angle_t::_MINUS_PI, Axis_t::X_AXIS));
    EXPECT_EQ(adapter->qubit_measure(0, false), 1);
}

TEST(StimAdapterTest, SqrtYRotationRoundTripMeasuresZero)
{
    // There is no H gate reachable through single_qubit_gate in this adapter
    // (see file-level comment above). SQRT_Y (angle=_8_PI_16, axis=Y_AXIS) is
    // the closest available analogue to H: applied to |0> it produces |+>
    // exactly (up to an irrelevant global phase), same as H would. Its adjoint
    // SQRT_Y_DAG (angle=_MINUS_8_PI_16, axis=Y_AXIS) rotates back to |0>.
    //
    // This also demonstrates the actual X-basis-measurement convention used by
    // this codebase: qubit_measure()'s `basis` argument does NOT itself rotate
    // anything (see the next test) -- the caller (normally SimulatorManager)
    // is expected to apply the basis-rotation gate itself via single_qubit_gate
    // before calling qubit_measure. Here we do that explicitly and get a
    // deterministic outcome.
    auto adapter = make_adapter(2);
    ASSERT_TRUE(adapter->qubit_initialize(0));
    ASSERT_TRUE(adapter->single_qubit_gate(0, Angle_t::_8_PI_16, Axis_t::Y_AXIS));       // |0> -> |+>
    ASSERT_TRUE(adapter->single_qubit_gate(0, Angle_t::_MINUS_8_PI_16, Axis_t::Y_AXIS)); // |+> -> |0>
    EXPECT_EQ(adapter->qubit_measure(0, true), 0);
}

TEST(StimAdapterTest, QubitMeasureBasisFlagDoesNotRotateState)
{
    // Pins down that qubit_measure always performs a Z-basis (do_MZ)
    // measurement in this adapter -- the `basis` parameter is not consulted
    // to apply any rotation. An untouched |0> qubit measures 0 regardless of
    // whether basis=false or basis=true is passed.
    auto adapter = make_adapter(2);
    ASSERT_TRUE(adapter->qubit_initialize(0));
    EXPECT_EQ(adapter->qubit_measure(0, true), 0);
}

TEST(StimAdapterTest, SingleQubitGateZeroAngleIsUnsupportedAndLeavesStateUnchanged)
{
    // No Angle_t::_0_PI_16 special case exists in single_qubit_gate: angle=0
    // fails to match _8_PI_16 / _MINUS_8_PI_16 / _MINUS_PI and falls through
    // to the final "else" branch ("Unsupported angle for single-qubit gate"),
    // returning false without ever invoking a Stim gate op on the qubit (the
    // quantum state itself is left untouched, even though the cached
    // single_target scratch array is harmlessly overwritten first).
    auto adapter = make_adapter(2);
    ASSERT_TRUE(adapter->qubit_initialize(0));
    EXPECT_FALSE(adapter->single_qubit_gate(0, Angle_t::_0_PI_16, Axis_t::X_AXIS));
    EXPECT_EQ(adapter->qubit_measure(0, false), 0);
}

// ---- single_qubit_noise ----

TEST(StimAdapterTest, SingleQubitNoiseWithZeroProbabilitiesReturnsTrueAndLeavesStateUnchanged)
{
    auto adapter = make_adapter(2);
    ASSERT_TRUE(adapter->qubit_initialize(0));
    EXPECT_TRUE(adapter->single_qubit_noise(0, 0.0f, 0.0f, 0.0f));
    EXPECT_EQ(adapter->qubit_measure(0, false), 0);
}

// ---- two_qubit_gate ----

TEST(StimAdapterTest, TwoQubitGateRejectsSameQubitAddress)
{
    auto adapter = make_adapter(2);
    ASSERT_TRUE(adapter->qubit_initialize(0));
    EXPECT_FALSE(adapter->two_qubit_gate(0, 0, Angle_t::_8_PI_16, Axis_t::X_AXIS));
}

TEST(StimAdapterTest, SqrtXXAppliedTwiceActsAsXXDeterministically)
{
    // SQRT_XX (angle=_8_PI_16, axis=X_AXIS) is Stim's exp(-i*pi/4 * X(x)X).
    // Its unitary squared works out to exactly X(x)X with no residual phase,
    // so applying it twice to |00> deterministically yields |11>. This is
    // the only genuinely-entangling two-qubit gate family two_qubit_gate
    // exposes (see the next test for the non-entangling _MINUS_PI branch).
    auto adapter = make_adapter(2);
    ASSERT_TRUE(adapter->qubit_initialize(0));
    ASSERT_TRUE(adapter->qubit_initialize(1));
    ASSERT_TRUE(adapter->two_qubit_gate(0, 1, Angle_t::_8_PI_16, Axis_t::X_AXIS));
    ASSERT_TRUE(adapter->two_qubit_gate(0, 1, Angle_t::_8_PI_16, Axis_t::X_AXIS));
    EXPECT_EQ(adapter->qubit_measure(0, false), 1);
    EXPECT_EQ(adapter->qubit_measure(1, false), 1);
}

TEST(StimAdapterTest, TwoQubitGateMinusPiAppliesIndependentXFlipsNotEntangling)
{
    // two_qubit_gate's angle=_MINUS_PI branch on X_AXIS does not implement any
    // real two-qubit interaction (there is no CX/CZ mapping anywhere in this
    // file). It simply applies a plain X gate to each qubit independently.
    // The result is deterministic but purely classical/non-entangling.
    auto adapter = make_adapter(2);
    ASSERT_TRUE(adapter->qubit_initialize(0));
    ASSERT_TRUE(adapter->qubit_initialize(1));
    ASSERT_TRUE(adapter->two_qubit_gate(0, 1, Angle_t::_MINUS_PI, Axis_t::X_AXIS));
    EXPECT_EQ(adapter->qubit_measure(0, false), 1);
    EXPECT_EQ(adapter->qubit_measure(1, false), 1);
}

TEST(StimAdapterTest, TwoQubitGateZAxisIsUnsupported)
{
    // Axis_t only defines X_AXIS/Y_AXIS/Z_AXIS, but two_qubit_gate has no
    // Z_AXIS branch at all -- it falls straight to "Invalid axis for
    // two-qubit gate" and returns false.
    auto adapter = make_adapter(2);
    ASSERT_TRUE(adapter->qubit_initialize(0));
    ASSERT_TRUE(adapter->qubit_initialize(1));
    EXPECT_FALSE(adapter->two_qubit_gate(0, 1, Angle_t::_8_PI_16, Axis_t::Z_AXIS));
}

TEST(StimAdapterTest, TwoQubitGateZeroAngleIsUnsupported)
{
    // Mirrors the single-qubit case: angle=0 matches none of _8_PI_16,
    // _MINUS_PI, _MINUS_8_PI_16 on X_AXIS (or Y_AXIS), so it falls through to
    // "Unsupported angle for two-qubit gate" and returns false, leaving both
    // qubits' state untouched.
    auto adapter = make_adapter(2);
    ASSERT_TRUE(adapter->qubit_initialize(0));
    ASSERT_TRUE(adapter->qubit_initialize(1));
    EXPECT_FALSE(adapter->two_qubit_gate(0, 1, Angle_t::_0_PI_16, Axis_t::X_AXIS));
    EXPECT_EQ(adapter->qubit_measure(0, false), 0);
    EXPECT_EQ(adapter->qubit_measure(1, false), 0);
}

// ---- qubit_swap ----

TEST(StimAdapterTest, QubitSwapExchangesQubitStates)
{
    auto adapter = make_adapter(2);
    ASSERT_TRUE(adapter->qubit_initialize(0));
    ASSERT_TRUE(adapter->qubit_initialize(1));
    ASSERT_TRUE(adapter->single_qubit_gate(0, Angle_t::_MINUS_PI, Axis_t::X_AXIS)); // qubit 0 -> |1>, qubit 1 stays |0>

    ASSERT_TRUE(adapter->qubit_swap(0, 1));

    EXPECT_EQ(adapter->qubit_measure(0, false), 0);
    EXPECT_EQ(adapter->qubit_measure(1, false), 1);
}

TEST(StimAdapterTest, QubitSwapRejectsSameQubitAddress)
{
    auto adapter = make_adapter(2);
    ASSERT_TRUE(adapter->qubit_initialize(0));
    EXPECT_FALSE(adapter->qubit_swap(0, 0));
}

// ---- qubit_move / entanglement_swap (currently unimplemented stubs) ----

TEST(StimAdapterTest, QubitMoveIsAnUnimplementedStubReturningFalse)
{
    // qubit_move has no real implementation: it unconditionally logs
    // "qubit_move is not implemented yet!" and returns false, regardless of
    // the given addresses. Pinning this down so a future accidental
    // implementation doesn't silently change behavior unnoticed.
    auto adapter = make_adapter(2);
    EXPECT_FALSE(adapter->qubit_move(0, 1));
}

TEST(StimAdapterTest, EntanglementSwapIsAnUnimplementedStubReturningFalse)
{
    // entanglement_swap's body is entirely commented out; it unconditionally
    // logs "entanglement_swap is not implemented yet!" and returns false.
    auto adapter = make_adapter(2);
    EXPECT_FALSE(adapter->entanglement_swap(0, 1));
}

// ---- allocate_qubits ----

TEST(StimAdapterTest, AllocateQubitsSucceedsExactlyAtMax)
{
    // MAX_QUBITS_AVAILABLE is 250 (private constant in stim_adapter.hpp).
    // Starting with 5 already-registered qubits, allocating 245 more reaches
    // exactly 250 and must succeed.
    auto adapter = make_adapter(5);
    EXPECT_TRUE(adapter->allocate_qubits(245));
}

TEST(StimAdapterTest, AllocateQubitsFailsJustOverMax)
{
    auto adapter = make_adapter(5);
    EXPECT_FALSE(adapter->allocate_qubits(246));
}

// ---- attempt_entanglement ----

TEST(StimAdapterTest, AttemptEntanglementRejectsSelfEntanglement)
{
    auto adapter = make_adapter(4);
    EXPECT_EQ(adapter->attempt_entanglement(/*fidelity=*/100, /*local_qubit=*/2,
                                             /*neighbour_address=*/2, /*is_initiator=*/true, /*qid=*/0),
              -1);
}

TEST(StimAdapterTest, AttemptEntanglementInitiatorProducesPsiPlusDeterministically)
{
    // The initiator branch force-initializes both qubits to |0>, applies
    // H (local) + X (neighbour) + CNOT(local->neighbour), producing
    // (|01>+|10>)/sqrt(2) = |Psi+>. For this exact state XX has expectation
    // +1 and ZZ has expectation -1 -- always, deterministically -- which the
    // implementation classifies as return code 2 (|Psi+>).
    auto adapter = make_adapter(4);
    EXPECT_EQ(adapter->attempt_entanglement(100, 0, 1, /*is_initiator=*/true, 0), 2);

    // |Psi+> is deterministically anti-correlated in the Z basis: exactly one
    // of the two qubits measures 1, even though each individual outcome is
    // (separately) random.
    const uint8_t m0 = adapter->qubit_measure(0, false);
    const uint8_t m1 = adapter->qubit_measure(1, false);
    EXPECT_NE(m0, m1);
}

TEST(StimAdapterTest, AttemptEntanglementResponderWithoutPriorEntanglementIsUndefined)
{
    // As a responder, no gates are applied at all -- attempt_entanglement
    // purely inspects the current XX/ZZ observables. On two qubits that were
    // never actually entangled (freshly initialized, i.e. |00>), XX has
    // expectation 0 (not a definite +-1 eigenvalue), so none of the four
    // Bell-state branches match and the implementation falls through to its
    // "Undefined Bell state" error path, returning -1.
    auto adapter = make_adapter(4);
    ASSERT_TRUE(adapter->qubit_initialize(2));
    ASSERT_TRUE(adapter->qubit_initialize(3));
    EXPECT_EQ(adapter->attempt_entanglement(100, 2, 3, /*is_initiator=*/false, 0), -1);
}
