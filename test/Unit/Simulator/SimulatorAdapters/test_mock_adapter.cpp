// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <gtest/gtest.h>

#include <limits>

#include "Simulator/SimulatorAdapters/mock_adapter.hpp"

// MockAdapter is shipped, production code (linked into the `qilab` binary for early
// timing/perf experiments) -- distinct from the test-only SpyAdapter used in
// test/Functional/Simulator/SimulatorManager/test_simulator_manager.cpp. Every method
// just logs to stdout and returns a fixed value regardless of input; these tests pin
// down those fixed return values and confirm no input (including boundary/extreme
// qubit addresses) causes a crash.

namespace {

TEST(MockAdapterTest, QubitInitializeAlwaysReturnsTrue) {
    MockAdapter adapter;
    EXPECT_TRUE(adapter.qubit_initialize(0));
    EXPECT_TRUE(adapter.qubit_initialize(std::numeric_limits<uint8_t>::max()));
}

TEST(MockAdapterTest, SingleQubitGateAlwaysReturnsTrue) {
    MockAdapter adapter;
    EXPECT_TRUE(adapter.single_qubit_gate(0, Angle_t::_0_PI_16, Axis_t::X_AXIS));
    EXPECT_TRUE(adapter.single_qubit_gate(std::numeric_limits<uint8_t>::max(),
                                           Angle_t::_MINUS_1_PI_16, Axis_t::Z_AXIS));
}

TEST(MockAdapterTest, SingleQubitNoiseAlwaysReturnsTrue) {
    MockAdapter adapter;
    EXPECT_TRUE(adapter.single_qubit_noise(0, 0.0f, 0.0f, 0.0f));
    EXPECT_TRUE(adapter.single_qubit_noise(1, 0.9f, 0.9f, 0.9f)); // no validation performed
}

TEST(MockAdapterTest, TwoQubitGateAlwaysReturnsTrue) {
    MockAdapter adapter;
    EXPECT_TRUE(adapter.two_qubit_gate(0, 1, Angle_t::_8_PI_16, Axis_t::Y_AXIS));
    EXPECT_TRUE(adapter.two_qubit_gate(3, 3, Angle_t::_0_PI_16, Axis_t::X_AXIS)); // no same-qubit check
}

TEST(MockAdapterTest, QubitMeasureAlwaysReturnsFixedByte) {
    MockAdapter adapter;
    EXPECT_EQ(adapter.qubit_measure(0, false), 0x08);
    EXPECT_EQ(adapter.qubit_measure(std::numeric_limits<uint8_t>::max(), true), 0x08);
}

TEST(MockAdapterTest, AttemptEntanglementAlwaysReturnsFixedCode) {
    MockAdapter adapter;
    EXPECT_EQ(adapter.attempt_entanglement(0, 0, 1, true, 0), 0x09);
    EXPECT_EQ(adapter.attempt_entanglement(65535, 3, 3, false, 2), 0x09); // no self-entanglement check
}

TEST(MockAdapterTest, EntanglementMeasureAlwaysReturnsFixedCode) {
    MockAdapter adapter;
    EXPECT_EQ(adapter.entanglement_measure(0, 1, false, true, 0, false, 0), 0x0B);
    EXPECT_EQ(adapter.entanglement_measure(65535, 2, true, false, 3, true, 255), 0x0B);
}

TEST(MockAdapterTest, PreMeasureGatesSelectionDoesNotCrash) {
    MockAdapter adapter;
    adapter.pre_measure_gates_selection(true, Angle_t::_PI_16, Angle_t::_2_PI_16, Angle_t::_3_PI_16);
    adapter.pre_measure_gates_selection(false, Angle_t::_0_PI_16, Angle_t::_0_PI_16, Angle_t::_0_PI_16);
    SUCCEED();
}

TEST(MockAdapterTest, QubitMoveAlwaysReturnsTrue) {
    MockAdapter adapter;
    EXPECT_TRUE(adapter.qubit_move(0, 1));
    EXPECT_TRUE(adapter.qubit_move(5, 5)); // no same-qubit check
}

TEST(MockAdapterTest, QubitSwapAlwaysReturnsTrue) {
    MockAdapter adapter;
    EXPECT_TRUE(adapter.qubit_swap(0, 1));
    EXPECT_TRUE(adapter.qubit_swap(5, 5));
}

TEST(MockAdapterTest, EntanglementSwapAlwaysReturnsTrue) {
    MockAdapter adapter;
    EXPECT_TRUE(adapter.entanglement_swap(0, 1));
    EXPECT_TRUE(adapter.entanglement_swap(5, 5));
}

TEST(MockAdapterTest, ApplyQubitNoiseAlwaysReturnsTrue) {
    MockAdapter adapter;
    EXPECT_TRUE(adapter.apply_qubit_noise(0, 0.0f, 0.0f));
    EXPECT_TRUE(adapter.apply_qubit_noise(1, 1.5f, -1.0f)); // no validation performed
}

TEST(MockAdapterTest, AllocateQubitsAlwaysReturnsTrue) {
    MockAdapter adapter;
    EXPECT_TRUE(adapter.allocate_qubits(0));
    EXPECT_TRUE(adapter.allocate_qubits(std::numeric_limits<uint8_t>::max())); // no bound enforced
}

} // namespace
