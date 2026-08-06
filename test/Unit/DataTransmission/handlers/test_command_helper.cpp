#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstring>

#include "DataTransmission/handlers/command_helper.hpp"
#include "DataTypes/commands.hpp"

namespace {

// BaseC::crc8_ccitt / crc8_ccitt_update are `protected static`. This helper
// exposes them as public (via a using-declaration) so the CRC implementation
// can be exercised directly, without needing a fully-populated command frame.
//
// Parameters confirmed from command_helper.hpp: poly = 0x07, init = 0x00,
// xorout = 0x00, MSB-first (no input/output reflection). Those parameters
// match the well-known "CRC-8/SMBUS" variant, whose published check value
// for the ASCII string "123456789" is 0xF4 - used below as the known-vector
// test.
struct CrcTestHelper : public BaseC
{
    using BaseC::crc8_ccitt;
    using BaseC::crc8_ccitt_update;

    std::size_t create_command(uint8_t *, std::size_t) const override { return 0; }
};

// ---------------------------------------------------------------------------
// CRC-8/CCITT (poly 0x07, init 0x00, xorout 0x00)
// ---------------------------------------------------------------------------

TEST(Crc8CcittTest, KnownVectorAsciiDigitsMatchesCrc8SmbusCheckValue)
{
    const uint8_t data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    EXPECT_EQ(CrcTestHelper::crc8_ccitt(data, sizeof(data)), 0xF4);
}

TEST(Crc8CcittTest, AllZeroInputYieldsZero)
{
    const uint8_t data[3] = {0x00, 0x00, 0x00};
    EXPECT_EQ(CrcTestHelper::crc8_ccitt(data, sizeof(data)), 0x00);
}

TEST(Crc8CcittTest, EmptyInputYieldsInit)
{
    EXPECT_EQ(CrcTestHelper::crc8_ccitt(nullptr, 0), 0x00);
}

TEST(Crc8CcittTest, SingleBitFlipAlwaysChangesChecksum)
{
    const uint8_t base[3] = {0x02, 0x0F, 0x5F};
    const uint8_t base_crc = CrcTestHelper::crc8_ccitt(base, sizeof(base));

    for (std::size_t byte_idx = 0; byte_idx < sizeof(base); ++byte_idx)
    {
        for (int bit = 0; bit < 8; ++bit)
        {
            uint8_t flipped[3];
            std::memcpy(flipped, base, sizeof(base));
            flipped[byte_idx] ^= static_cast<uint8_t>(1u << bit);

            const uint8_t flipped_crc = CrcTestHelper::crc8_ccitt(flipped, sizeof(flipped));
            EXPECT_NE(flipped_crc, base_crc) << "byte " << byte_idx << " bit " << bit;
        }
    }
}

// ---------------------------------------------------------------------------
// Nop
// ---------------------------------------------------------------------------

TEST(NopTest, BuildsAllZeroFrame)
{
    Nop cmd;
    std::array<uint8_t, 4> buff{};

    EXPECT_EQ(cmd.create_command(buff.data(), buff.size()), 4u);
    EXPECT_EQ(buff[0], Command_t::NOP);
    EXPECT_EQ(buff[1], 0x00);
    EXPECT_EQ(buff[2], 0x00);
    EXPECT_EQ(buff[3], 0x00); // CRC of {0,0,0} is 0 for this CRC variant.
}

TEST(NopTest, RefusesUndersizedBuffer)
{
    Nop cmd;
    std::array<uint8_t, 3> buff{};

    EXPECT_EQ(cmd.create_command(buff.data(), buff.size()), 0u);
}

TEST(NopTest, RefusesNullBuffer)
{
    Nop cmd;

    EXPECT_EQ(cmd.create_command(nullptr, 4), 0u);
}

// ---------------------------------------------------------------------------
// InitQubit
// ---------------------------------------------------------------------------

TEST(InitQubitTest, PacksAddressIntoQubit1Mask)
{
    InitQubit cmd(0x0F); // max address that fits in QUBIT_1_BIT_MASK (0x0f)
    std::array<uint8_t, 4> buff{};

    EXPECT_EQ(cmd.create_command(buff.data(), buff.size()), 4u);
    EXPECT_EQ(buff[0], Command_t::QUBIT_INIT);
    EXPECT_EQ(buff[1], 0x0F);
    EXPECT_EQ(buff[2], 0x00);
    EXPECT_EQ(buff[3], 0xA8);
}

TEST(InitQubitTest, MidRangeAddress)
{
    InitQubit cmd(0x05);
    std::array<uint8_t, 4> buff{};

    ASSERT_EQ(cmd.create_command(buff.data(), buff.size()), 4u);
    EXPECT_EQ(buff[1], 0x05);
    EXPECT_EQ(buff[3], 0x2A);
}

TEST(InitQubitTest, OutOfRangeAddressIsMaskedNotClamped)
{
    // addr = 0xFF overflows the 4-bit field; only the low nibble survives,
    // so it must produce the same frame as addr = 0x0F.
    InitQubit cmd(0xFF);
    std::array<uint8_t, 4> buff{};

    ASSERT_EQ(cmd.create_command(buff.data(), buff.size()), 4u);
    EXPECT_EQ(buff[1], 0x0F);
    EXPECT_EQ(buff[3], 0xA8);
}

// ---------------------------------------------------------------------------
// SingleQGate
// ---------------------------------------------------------------------------

TEST(SingleQGateTest, PacksAddressAngleAndAxis)
{
    SingleQGate cmd(0x0F, Angle_t::_MINUS_1_PI_16 /* 31, max angle */, Axis_t::Z_AXIS);
    std::array<uint8_t, 4> buff{};

    ASSERT_EQ(cmd.create_command(buff.data(), buff.size()), 4u);
    EXPECT_EQ(buff[0], Command_t::SINGLE_QUBIT_GATE);
    EXPECT_EQ(buff[1], 0x0F);
    EXPECT_EQ(buff[2], 0x5F); // (Z_AXIS(2) << 5) | 31
    EXPECT_EQ(buff[3], 0x8F);
}

TEST(SingleQGateTest, MinimumAngleAndAxis)
{
    SingleQGate cmd(0x05, Angle_t::_0_PI_16, Axis_t::X_AXIS);
    std::array<uint8_t, 4> buff{};

    ASSERT_EQ(cmd.create_command(buff.data(), buff.size()), 4u);
    EXPECT_EQ(buff[1], 0x05);
    EXPECT_EQ(buff[2], 0x00);
    EXPECT_EQ(buff[3], 0x97);
}

TEST(SingleQGateTest, MidRangeAngleAndYAxis)
{
    SingleQGate cmd(0x0F, Angle_t::_MINUS_15_PI_16 /* 17 */, Axis_t::Y_AXIS);
    std::array<uint8_t, 4> buff{};

    ASSERT_EQ(cmd.create_command(buff.data(), buff.size()), 4u);
    EXPECT_EQ(buff[1], 0x0F);
    EXPECT_EQ(buff[2], 0x31); // (Y_AXIS(1) << 5) | 17
    EXPECT_EQ(buff[3], 0x82);
}

// ---------------------------------------------------------------------------
// TwoQGate
// ---------------------------------------------------------------------------

TEST(TwoQGateTest, PacksBothAddressesAngleAndAxis)
{
    TwoQGate cmd(0x0F, 0x0F, Angle_t::_MINUS_1_PI_16 /* 31 */, Axis_t::Z_AXIS);
    std::array<uint8_t, 4> buff{};

    ASSERT_EQ(cmd.create_command(buff.data(), buff.size()), 4u);
    EXPECT_EQ(buff[0], Command_t::TWO_QUBIT_GATE);
    EXPECT_EQ(buff[1], 0xFF); // addr1 in low nibble, addr2 in high nibble
    EXPECT_EQ(buff[2], 0x5F);
    EXPECT_EQ(buff[3], 0xF0);
}

TEST(TwoQGateTest, DistinctAddressesMidRangeAngle)
{
    TwoQGate cmd(0x03, 0x0A, Angle_t::_5_PI_16, Axis_t::Y_AXIS);
    std::array<uint8_t, 4> buff{};

    ASSERT_EQ(cmd.create_command(buff.data(), buff.size()), 4u);
    EXPECT_EQ(buff[1], 0xA3); // (0x0A << 4) | 0x03
    EXPECT_EQ(buff[2], 0x25); // (Y_AXIS(1) << 5) | 5
    EXPECT_EQ(buff[3], 0x61);
}

// ---------------------------------------------------------------------------
// QMeasure
// ---------------------------------------------------------------------------

TEST(QMeasureTest, PacksAddressAndBasisTrue)
{
    QMeasure cmd(0x0F, true, Axis_t::X_AXIS); // axis is accepted but not stored/packed
    std::array<uint8_t, 4> buff{};

    ASSERT_EQ(cmd.create_command(buff.data(), buff.size()), 4u);
    EXPECT_EQ(buff[0], Command_t::QUBIT_MEASUREMENT);
    EXPECT_EQ(buff[1], 0x0F);
    EXPECT_EQ(buff[2], 0x01);
    EXPECT_EQ(buff[3], 0x6F);
}

TEST(QMeasureTest, PacksAddressAndBasisFalse)
{
    QMeasure cmd(0x0F, false, Axis_t::X_AXIS);
    std::array<uint8_t, 4> buff{};

    ASSERT_EQ(cmd.create_command(buff.data(), buff.size()), 4u);
    EXPECT_EQ(buff[1], 0x0F);
    EXPECT_EQ(buff[2], 0x00);
    EXPECT_EQ(buff[3], 0x68);
}

// ---------------------------------------------------------------------------
// Entangle
// ---------------------------------------------------------------------------

TEST(EntangleTest, PacksFidelityNeighbourAndQid)
{
    // qid is passed through as a whole byte (see the WORKAROUND comment in
    // command_helper.hpp), so 0xFF is a valid boundary value, not overflow.
    Entangle cmd(0x0F, 0x0F, 0xFF);
    std::array<uint8_t, 4> buff{};

    ASSERT_EQ(cmd.create_command(buff.data(), buff.size()), 4u);
    EXPECT_EQ(buff[0], Command_t::ENTANGLEMENT_ATTEMPT);
    EXPECT_EQ(buff[1], 0xFF); // (nghbr << 4) | fidelity, both maxed at 0x0F
    EXPECT_EQ(buff[2], 0xFF);
    EXPECT_EQ(buff[3], 0xE4);
}

TEST(EntangleTest, MidRangeValues)
{
    Entangle cmd(0x05, 0x03, 0x02);
    std::array<uint8_t, 4> buff{};

    ASSERT_EQ(cmd.create_command(buff.data(), buff.size()), 4u);
    EXPECT_EQ(buff[1], 0x35); // (0x03 << 4) | 0x05
    EXPECT_EQ(buff[2], 0x02);
    EXPECT_EQ(buff[3], 0x76);
}

TEST(EntangleTest, FidelityAndNeighbourOverflowAreMaskedNotClamped)
{
    // fidelity/nghbr overflowing their 4-bit fields must be masked down to
    // the same result as the max in-range values (0x0F each).
    Entangle cmd(0xFF, 0xFF, 0xFF);
    std::array<uint8_t, 4> buff{};

    ASSERT_EQ(cmd.create_command(buff.data(), buff.size()), 4u);
    EXPECT_EQ(buff[1], 0xFF);
}

// ---------------------------------------------------------------------------
// EntanMeasure
// ---------------------------------------------------------------------------

TEST(EntanMeasureTest, PacksAllFieldsAtMaximum)
{
    // repeat: 4 bits (0x0F), qid: 2 bits fitting exactly in QID_BIT_MASK (0x03
    // -> shifted to 0x0c), now/basis: 1 bit each.
    EntanMeasure cmd(0x0F, 0x0F, 0x03, true, true, 0x0F);
    std::array<uint8_t, 4> buff{};

    ASSERT_EQ(cmd.create_command(buff.data(), buff.size()), 4u);
    EXPECT_EQ(buff[0], Command_t::ENTANGLEMENT_MEASURE);
    EXPECT_EQ(buff[1], 0xFF);
    EXPECT_EQ(buff[2], 0xFF);
    EXPECT_EQ(buff[3], 0x59);
}

TEST(EntanMeasureTest, PacksAllFieldsAtMinimumWithMidRangeIdentifiers)
{
    EntanMeasure cmd(0x05, 0x02, 0x01, false, false, 0x03);
    std::array<uint8_t, 4> buff{};

    ASSERT_EQ(cmd.create_command(buff.data(), buff.size()), 4u);
    EXPECT_EQ(buff[1], 0x25); // (0x02 << 4) | 0x05
    EXPECT_EQ(buff[2], 0x34); // (0x03 << 4) | (0x01 << 2) | (0 << 1) | 0
    EXPECT_EQ(buff[3], 0x1E);
}

// ---------------------------------------------------------------------------
// PreMeasureGateSelect
// ---------------------------------------------------------------------------

TEST(PreMeasureGateSelectTest, PacksAllThreeAnglesAndTargetAtMaximum)
{
    PreMeasureGateSelect cmd(Angle_t::_MINUS_1_PI_16 /* 31 */,
                              Angle_t::_MINUS_1_PI_16 /* 31 */,
                              Angle_t::_MINUS_1_PI_16 /* 31 */,
                              true);
    std::array<uint8_t, 4> buff{};

    ASSERT_EQ(cmd.create_command(buff.data(), buff.size()), 4u);
    EXPECT_EQ(buff[0], Command_t::PRE_MEASUREMENT_GATES_SELECTION);
    EXPECT_EQ(buff[1], 0xDF);
    EXPECT_EQ(buff[2], 0xEF);
    EXPECT_EQ(buff[3], 0xEC);
}

TEST(PreMeasureGateSelectTest, PacksAllZeroAnglesAndFalseTarget)
{
    PreMeasureGateSelect cmd(Angle_t::_0_PI_16, Angle_t::_0_PI_16, Angle_t::_0_PI_16, false);
    std::array<uint8_t, 4> buff{};

    ASSERT_EQ(cmd.create_command(buff.data(), buff.size()), 4u);
    EXPECT_EQ(buff[1], 0x00);
    EXPECT_EQ(buff[2], 0x00);
    EXPECT_EQ(buff[3], 0x16);
}

TEST(PreMeasureGateSelectTest, PacksMixedMidRangeAngles)
{
    PreMeasureGateSelect cmd(Angle_t::_5_PI_16, Angle_t::_9_PI_16, Angle_t::_MINUS_15_PI_16 /* 17 */, true);
    std::array<uint8_t, 4> buff{};

    ASSERT_EQ(cmd.create_command(buff.data(), buff.size()), 4u);
    EXPECT_EQ(buff[1], 0x05);
    EXPECT_EQ(buff[2], 0xC5);
    EXPECT_EQ(buff[3], 0x02);
}

// ---------------------------------------------------------------------------
// QSwap / QMove / EntangleSwap (identical packing, distinct opcodes)
// ---------------------------------------------------------------------------

TEST(QSwapTest, PacksBothAddressesAtMaximum)
{
    QSwap cmd(0x0F, 0x0F);
    std::array<uint8_t, 4> buff{};

    ASSERT_EQ(cmd.create_command(buff.data(), buff.size()), 4u);
    EXPECT_EQ(buff[0], Command_t::QUBIT_SWAP);
    EXPECT_EQ(buff[1], 0xFF);
    EXPECT_EQ(buff[2], 0x00);
    EXPECT_EQ(buff[3], 0xED);
}

TEST(QSwapTest, PacksDistinctAddresses)
{
    QSwap cmd(0x03, 0x0A);
    std::array<uint8_t, 4> buff{};

    ASSERT_EQ(cmd.create_command(buff.data(), buff.size()), 4u);
    EXPECT_EQ(buff[1], 0xA3);
    EXPECT_EQ(buff[3], 0x1D);
}

TEST(QMoveTest, PacksBothAddressesAtMaximum)
{
    QMove cmd(0x0F, 0x0F);
    std::array<uint8_t, 4> buff{};

    ASSERT_EQ(cmd.create_command(buff.data(), buff.size()), 4u);
    EXPECT_EQ(buff[0], Command_t::QUBIT_MOVE);
    EXPECT_EQ(buff[1], 0xFF);
    EXPECT_EQ(buff[2], 0x00);
    EXPECT_EQ(buff[3], 0x86);
}

TEST(QMoveTest, PacksDistinctAddresses)
{
    QMove cmd(0x02, 0x07);
    std::array<uint8_t, 4> buff{};

    ASSERT_EQ(cmd.create_command(buff.data(), buff.size()), 4u);
    EXPECT_EQ(buff[1], 0x72);
    EXPECT_EQ(buff[3], 0xD9);
}

TEST(EntangleSwapTest, PacksBothAddressesAtMaximum)
{
    EntangleSwap cmd(0x0F, 0x0F);
    std::array<uint8_t, 4> buff{};

    ASSERT_EQ(cmd.create_command(buff.data(), buff.size()), 4u);
    EXPECT_EQ(buff[0], Command_t::ENTANGLEMENT_SWAP);
    EXPECT_EQ(buff[1], 0xFF);
    EXPECT_EQ(buff[2], 0x00);
    EXPECT_EQ(buff[3], 0x50);
}

TEST(EntangleSwapTest, PacksDistinctAddresses)
{
    EntangleSwap cmd(0x01, 0x0C);
    std::array<uint8_t, 4> buff{};

    ASSERT_EQ(cmd.create_command(buff.data(), buff.size()), 4u);
    EXPECT_EQ(buff[1], 0xC1);
    EXPECT_EQ(buff[3], 0x7F);
}

// ---------------------------------------------------------------------------
// ConditionalCommand
// ---------------------------------------------------------------------------

TEST(ConditionalCommandTest, DelegatesFrameBuildingToWrappedCommand)
{
    InitQubit inner(0x05);
    ConditionalCommand cmd([](const int &) { return true; }, &inner);

    std::array<uint8_t, 4> buff{};
    EXPECT_EQ(cmd.create_command(buff.data(), buff.size()), 4u);
    EXPECT_EQ(buff[0], Command_t::QUBIT_INIT);
    EXPECT_EQ(buff[1], 0x05);
    EXPECT_EQ(buff[3], 0x2A);
}

TEST(ConditionalCommandTest, ConditionalExecutionReflectsPredicateResult)
{
    InitQubit inner(0x05);
    ConditionalCommand allow([](const int &cntx) { return cntx > 0; }, &inner);
    ConditionalCommand deny([](const int &cntx) { return cntx > 0; }, &inner);

    EXPECT_TRUE(allow.conditional_execution(1));
    EXPECT_FALSE(deny.conditional_execution(-1));
}

TEST(ConditionalCommandTest, ReturnsFourEvenWhenInnerCommandFails)
{
    // The wrapper does not propagate the inner command's own return value;
    // it always reports 4 bytes written, even if the inner call refused to
    // write anything (e.g. because it was handed a null buffer).
    InitQubit inner(0x05);
    ConditionalCommand cmd([](const int &) { return true; }, &inner);

    EXPECT_EQ(cmd.create_command(nullptr, 4), 4u);
}

} // namespace
