#include <gtest/gtest.h>

#include "DataTypes/commands.hpp"

// Existing coverage (e.g. test_transmission_processor_sync.cpp, test_entanglement_manager.cpp)
// only ever constructs these commands from zero-filled buffers to check command_type
// dispatch. These tests exercise the actual bit-mask/shift decode logic in each
// CommandBase-derived constructor with concrete, hand-verified non-zero bit patterns,
// including cases where two fields are packed into the same byte (or, for
// PreMeasureGatesSelection, across two bytes) to catch any cross-talk between them.

namespace {

TEST(CommandBaseTest, DecodesCommandTypeAndCrcFromData0AndData3) {
    uint8_t data[4] = {static_cast<uint8_t>(Command_t::QUBIT_INIT), 0x00, 0x00, 0xAB};
    QubitInitialize cmd(data);
    EXPECT_EQ(cmd.command_type, Command_t::QUBIT_INIT);
    EXPECT_EQ(cmd.crc, 0xAB);
}

TEST(CommandBaseTest, CalculateFidelityMacroMatchesFormula) {
    EXPECT_EQ(CALCULATE_FIDELITY(0), 50);
    EXPECT_EQ(CALCULATE_FIDELITY(15), 125); // max 4-bit nibble: 50 + 15*5
    EXPECT_EQ(CALCULATE_FIDELITY(1), 55);
}

// ---- QubitInitialize ---------------------------------------------------------

TEST(QubitInitializeTest, DecodesQubit1FromLowNibble) {
    uint8_t data[4] = {Command_t::QUBIT_INIT, 0x0A /* qubit1=10, high nibble unused */, 0x00, 0x00};
    QubitInitialize cmd(data);
    EXPECT_EQ(cmd.qubit1_address, 10);
}

TEST(QubitInitializeTest, HighNibbleOfData1DoesNotLeakIntoQubit1) {
    uint8_t data[4] = {Command_t::QUBIT_INIT, 0xF0 /* qubit1=0, high nibble maxed */, 0x00, 0x00};
    QubitInitialize cmd(data);
    EXPECT_EQ(cmd.qubit1_address, 0);
}

TEST(QubitInitializeTest, Qubit1AtMaxNibbleValue) {
    uint8_t data[4] = {Command_t::QUBIT_INIT, 0x0F, 0x00, 0x00};
    QubitInitialize cmd(data);
    EXPECT_EQ(cmd.qubit1_address, 15);
}

// ---- SingleQubitGate ----------------------------------------------------------
// data[1]: qubit1 = low nibble (bits0-3), high nibble unused.
// data[2]: angle = bits0-4 (ANGLE_BIT_MASK=0x1f), axis = bits5-6 (AXIS_BIT_MASK=0x60) >> 5.

TEST(SingleQubitGateTest, DecodesQubitAngleAxisIndependently) {
    // angle = _9_PI_16 (9 = 0b01001), axis = Y_AXIS (1) -> byte = (1<<5)|9 = 0x29
    uint8_t data[4] = {Command_t::SINGLE_QUBIT_GATE, 0x07, 0x29, 0x00};
    SingleQubitGate cmd(data);
    EXPECT_EQ(cmd.qubit1_address, 7);
    EXPECT_EQ(cmd.angle, Angle_t::_9_PI_16);
    EXPECT_EQ(cmd.axis, Axis_t::Y_AXIS);
}

TEST(SingleQubitGateTest, AngleAtMaxFiveBitValueDoesNotLeakIntoAxis) {
    // angle = 31 (_MINUS_1_PI_16, max 5-bit value 0x1f), axis bits left at 0 (X_AXIS)
    uint8_t data[4] = {Command_t::SINGLE_QUBIT_GATE, 0x00, 0x1F, 0x00};
    SingleQubitGate cmd(data);
    EXPECT_EQ(cmd.angle, Angle_t::_MINUS_1_PI_16);
    EXPECT_EQ(cmd.axis, Axis_t::X_AXIS);
}

TEST(SingleQubitGateTest, AxisBitsDoNotLeakIntoAngle) {
    // axis = Z_AXIS (2 -> bits5-6 = 0b10 -> 0x40), angle bits left at 0
    uint8_t data[4] = {Command_t::SINGLE_QUBIT_GATE, 0x00, 0x40, 0x00};
    SingleQubitGate cmd(data);
    EXPECT_EQ(cmd.angle, Angle_t::_0_PI_16);
    EXPECT_EQ(cmd.axis, Axis_t::Z_AXIS);
}

// ---- TwoQubitGate --------------------------------------------------------------
// data[1]: qubit1 = low nibble, qubit2 = high nibble (QUBIT_2_BIT_MASK=0xf0) >> 4.

TEST(TwoQubitGateTest, DecodesBothQubitsFromOppositeNibblesWithoutCrosstalk) {
    // qubit1=3 (low nibble), qubit2=12 (high nibble) -> byte = (12<<4)|3 = 0xC3
    uint8_t data[4] = {Command_t::TWO_QUBIT_GATE, 0xC3, 0x00, 0x00};
    TwoQubitGate cmd(data);
    EXPECT_EQ(cmd.qubit1_address, 3);
    EXPECT_EQ(cmd.qubit2_address, 12);
}

TEST(TwoQubitGateTest, BothQubitAddressesAtMaxNibbleValue) {
    uint8_t data[4] = {Command_t::TWO_QUBIT_GATE, 0xFF, 0x00, 0x00};
    TwoQubitGate cmd(data);
    EXPECT_EQ(cmd.qubit1_address, 15);
    EXPECT_EQ(cmd.qubit2_address, 15);
}

TEST(TwoQubitGateTest, DecodesAngleAndAxisSameAsSingleQubitGate) {
    // angle = _3_PI_16 (3), axis = Y_AXIS (1) -> byte = (1<<5)|3 = 0x23
    uint8_t data[4] = {Command_t::TWO_QUBIT_GATE, 0x00, 0x23, 0x00};
    TwoQubitGate cmd(data);
    EXPECT_EQ(cmd.angle, Angle_t::_3_PI_16);
    EXPECT_EQ(cmd.axis, Axis_t::Y_AXIS);
}

// ---- QubitMeasure ---------------------------------------------------------------

TEST(QubitMeasureTest, DecodesQubitAndBasisBitIndependently) {
    // qubit1 = 5, basis bit (bit0 of data[2]) = 1
    uint8_t data[4] = {Command_t::QUBIT_MEASUREMENT, 0x05, 0x01, 0x00};
    QubitMeasure cmd(data);
    EXPECT_EQ(cmd.qubit1_address, 5);
    EXPECT_EQ(cmd.basis, 1);
}

TEST(QubitMeasureTest, HigherBitsOfData2DoNotLeakIntoBasis) {
    // Every bit except bit0 set in data[2]; basis must still read 0.
    uint8_t data[4] = {Command_t::QUBIT_MEASUREMENT, 0x00, 0xFE, 0x00};
    QubitMeasure cmd(data);
    EXPECT_EQ(cmd.basis, 0);
}

// ---- EntanglementAttempt ---------------------------------------------------------
// data[1]: fidelity = low nibble, nghbr = (high nibble) - 1 ("adjusting for 0-based indexing").
// data[2]: qid = the whole byte, unmasked (documented WORKAROUND in the source).

TEST(EntanglementAttemptTest, DecodesFidelityAndNeighbourFromOppositeNibbles) {
    // fidelity=7 (low nibble), wire nghbr nibble=5 -> decoded nghbr = 5-1 = 4
    uint8_t data[4] = {Command_t::ENTANGLEMENT_ATTEMPT, 0x57, 0xAB, 0x00};
    EntanglementAttempt cmd(data);
    EXPECT_EQ(cmd.fidelity, 7);
    EXPECT_EQ(cmd.nghbr, 4);
    EXPECT_EQ(cmd.qid, 0xAB); // whole byte, not masked
}

TEST(EntanglementAttemptTest, RawNeighbourWireValueZeroWrapsToUint8Max) {
    // High nibble of data[1] = 0 -> nghbr = 0 - 1, which wraps for the uint8_t member.
    // This pins down the CURRENT behavior of the "-1" adjustment at its zero boundary,
    // not a claim that wraparound is the intended design.
    uint8_t data[4] = {Command_t::ENTANGLEMENT_ATTEMPT, 0x00, 0x00, 0x00};
    EntanglementAttempt cmd(data);
    EXPECT_EQ(cmd.nghbr, 255);
}

TEST(EntanglementAttemptTest, RawNeighbourWireValueOneDecodesToZero) {
    uint8_t data[4] = {Command_t::ENTANGLEMENT_ATTEMPT, 0x10, 0x00, 0x00};
    EntanglementAttempt cmd(data);
    EXPECT_EQ(cmd.nghbr, 0);
}

// ---- EntangleMeasure --------------------------------------------------------------
// data[2] bits perfectly tile the byte: bit0=basis, bit1=immediate, bits2-3=qid, bits4-7=repetitions.

TEST(EntangleMeasureTest, DecodesAllFourData2FieldsWithoutCrosstalk) {
    // basis=1 (bit0), immediate=1 (bit1), qid=2 (bits2-3 = 0b10), repetitions=9 (bits4-7)
    // byte = (9<<4) | (2<<2) | (1<<1) | 1 = 0x90 | 0x08 | 0x02 | 0x01 = 0x9B
    uint8_t data[4] = {Command_t::ENTANGLEMENT_MEASURE, 0x30 /* fidelity=0, nghbr wire=3->2 */, 0x9B, 0x00};
    EntangleMeasure cmd(data);
    EXPECT_EQ(cmd.nghbr, 2);
    EXPECT_EQ(cmd.basis, 1);
    EXPECT_TRUE(cmd.immediate);
    EXPECT_EQ(cmd.qid, 2);
    EXPECT_EQ(cmd.repetitions, 9);
}

TEST(EntangleMeasureTest, OnlyQidBitsSetLeavesOtherThreeFieldsZero) {
    // qid bits (2-3) = 0b11 = 3, everything else 0 -> byte = 3<<2 = 0x0C
    uint8_t data[4] = {Command_t::ENTANGLEMENT_MEASURE, 0x00, 0x0C, 0x00};
    EntangleMeasure cmd(data);
    EXPECT_EQ(cmd.qid, 3);
    EXPECT_EQ(cmd.basis, 0);
    EXPECT_FALSE(cmd.immediate);
    EXPECT_EQ(cmd.repetitions, 0);
}

TEST(EntangleMeasureTest, RepetitionsAtMaxNibbleDoesNotLeakIntoQid) {
    uint8_t data[4] = {Command_t::ENTANGLEMENT_MEASURE, 0x00, 0xF0, 0x00};
    EntangleMeasure cmd(data);
    EXPECT_EQ(cmd.repetitions, 15);
    EXPECT_EQ(cmd.qid, 0);
    EXPECT_EQ(cmd.basis, 0);
    EXPECT_FALSE(cmd.immediate);
}

// ---- PreMeasureGatesSelection -----------------------------------------------------
// The one field genuinely packed ACROSS two bytes: angle2's low 3 bits live in the top
// 3 bits of data[1] (bits5-7), and its high 2 bits live in the bottom 2 bits of data[2]
// (bits0-1). data[1] bits0-4 = angle1; data[2] bits2-6 = angle3, bit7 = target.
// Together data[1] (angle1 + angle2-low) and data[2] (angle2-high + angle3 + target)
// each tile their whole byte with no gaps, so this is the strongest cross-talk test.

TEST(PreMeasureGatesSelectionTest, DecodesAllFourFieldsFromAHandVerifiedFrame) {
    // Intended: angle1=5, angle2=25, angle3=9, target=true.
    // angle2=25=0b11001 splits into low3=0b001=1 (-> data[1] bits5-7) and high2=0b11=3
    // (-> data[2] bits0-1).
    // data[1] = (1<<5) | 5              = 0b00100101 = 0x25
    // data[2] = (1<<7) | (9<<2) | 3     = 0b10100111 = 0xA7
    uint8_t data[4] = {Command_t::PRE_MEASUREMENT_GATES_SELECTION, 0x25, 0xA7, 0x00};
    PreMeasureGatesSelection cmd(data);
    EXPECT_EQ(cmd.angle1, Angle_t::_5_PI_16);
    EXPECT_EQ(cmd.angle2, Angle_t::_MINUS_7_PI_16); // raw value 25
    EXPECT_EQ(cmd.angle3, Angle_t::_9_PI_16);
    EXPECT_TRUE(cmd.target);
}

TEST(PreMeasureGatesSelectionTest, OnlyAngle1BitsSetLeavesAngle2Angle3TargetAtZero) {
    // angle1 = 31 (max 5 bits, 0x1f), everything else 0.
    uint8_t data[4] = {Command_t::PRE_MEASUREMENT_GATES_SELECTION, 0x1F, 0x00, 0x00};
    PreMeasureGatesSelection cmd(data);
    EXPECT_EQ(cmd.angle1, Angle_t::_MINUS_1_PI_16); // raw value 31
    EXPECT_EQ(cmd.angle2, Angle_t::_0_PI_16);
    EXPECT_EQ(cmd.angle3, Angle_t::_0_PI_16);
    EXPECT_FALSE(cmd.target);
}

TEST(PreMeasureGatesSelectionTest, OnlyAngle2CrossByteBitsSetLeavesAngle1Angle3TargetAtZero) {
    // angle2 = 31 (max 5 bits): low3=0b111=7 -> data[1] bits5-7; high2=0b11=3 -> data[2] bits0-1.
    // data[1] = 7<<5 = 0xE0 (angle1 bits0-4 = 0)
    // data[2] = 3    (angle3 bits2-6 = 0, target bit7 = 0)
    uint8_t data[4] = {Command_t::PRE_MEASUREMENT_GATES_SELECTION, 0xE0, 0x03, 0x00};
    PreMeasureGatesSelection cmd(data);
    EXPECT_EQ(cmd.angle1, Angle_t::_0_PI_16);
    EXPECT_EQ(cmd.angle2, Angle_t::_MINUS_1_PI_16); // raw value 31
    EXPECT_EQ(cmd.angle3, Angle_t::_0_PI_16);
    EXPECT_FALSE(cmd.target);
}

TEST(PreMeasureGatesSelectionTest, OnlyAngle3BitsSetLeavesOthersAtZero) {
    // angle3 = 31 (max 5 bits) -> data[2] bits2-6 = 0x7c; target bit7 = 0.
    uint8_t data[4] = {Command_t::PRE_MEASUREMENT_GATES_SELECTION, 0x00, 0x7C, 0x00};
    PreMeasureGatesSelection cmd(data);
    EXPECT_EQ(cmd.angle1, Angle_t::_0_PI_16);
    EXPECT_EQ(cmd.angle2, Angle_t::_0_PI_16);
    EXPECT_EQ(cmd.angle3, Angle_t::_MINUS_1_PI_16); // raw value 31
    EXPECT_FALSE(cmd.target);
}

TEST(PreMeasureGatesSelectionTest, OnlyTargetBitSetLeavesAllAnglesAtZero) {
    uint8_t data[4] = {Command_t::PRE_MEASUREMENT_GATES_SELECTION, 0x00, 0x80, 0x00};
    PreMeasureGatesSelection cmd(data);
    EXPECT_EQ(cmd.angle1, Angle_t::_0_PI_16);
    EXPECT_EQ(cmd.angle2, Angle_t::_0_PI_16);
    EXPECT_EQ(cmd.angle3, Angle_t::_0_PI_16);
    EXPECT_TRUE(cmd.target);
}

// ---- QubitMove / QubitSwap / EntanglementSwap -------------------------------------
// All three share TwoQubitGate's qubit1(low nibble)/qubit2(high nibble) decode with no
// angle/axis fields.

TEST(QubitMoveTest, DecodesBothQubitsFromOppositeNibbles) {
    uint8_t data[4] = {Command_t::QUBIT_MOVE, 0x3A, 0x00, 0x00}; // qubit1=10, qubit2=3
    QubitMove cmd(data);
    EXPECT_EQ(cmd.qubit1_address, 10);
    EXPECT_EQ(cmd.qubit2_address, 3);
}

TEST(QubitSwapTest, DecodesBothQubitsFromOppositeNibbles) {
    uint8_t data[4] = {Command_t::QUBIT_SWAP, 0x3A, 0x00, 0x00};
    QubitSwap cmd(data);
    EXPECT_EQ(cmd.qubit1_address, 10);
    EXPECT_EQ(cmd.qubit2_address, 3);
}

TEST(EntanglementSwapTest, DecodesBothQubitsFromOppositeNibbles) {
    uint8_t data[4] = {Command_t::ENTANGLEMENT_SWAP, 0x3A, 0x00, 0x00};
    EntanglementSwap cmd(data);
    EXPECT_EQ(cmd.qubit1_address, 10);
    EXPECT_EQ(cmd.qubit2_address, 3);
}

} // namespace
