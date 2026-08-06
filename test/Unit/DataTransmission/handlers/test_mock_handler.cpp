#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>

#include "DataTransmission/handlers/mock_handler.hpp"
#include "DataTypes/commands.hpp"

// MockHandler is a shipped ITransmissionHandler implementation that cycles through a
// fixed, hardcoded command sequence (InitQubit(0) -> SingleQGate(0, _MINUS_PI, Z_AXIS)
// -> QMeasure(0, false, Z_AXIS), per its create_commands()) each time receive_data()/
// transfer() is called, wrapping back to the start after the third command. These
// tests decode the emitted 4-byte frames with the real DataTypes/commands.hpp structs
// to confirm both the cycling behavior and the CRC each frame carries.

namespace {

uint8_t crc8_ccitt(const uint8_t *data, std::size_t len) {
    uint8_t crc = 0x00;
    for (std::size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x07) : static_cast<uint8_t>(crc << 1);
        }
    }
    return crc;
}

TEST(MockHandlerTest, ReceiveDataCyclesThroughInitGateMeasureThenWraps) {
    MockHandler handler;
    uint8_t frame[4];

    ASSERT_EQ(handler.receive_data(frame, 4), 0);
    QubitInitialize init(frame);
    EXPECT_EQ(init.command_type, Command_t::QUBIT_INIT);
    EXPECT_EQ(init.qubit1_address, 0);
    EXPECT_EQ(frame[3], crc8_ccitt(frame, 3));

    ASSERT_EQ(handler.receive_data(frame, 4), 0);
    SingleQubitGate gate(frame);
    EXPECT_EQ(gate.command_type, Command_t::SINGLE_QUBIT_GATE);
    EXPECT_EQ(gate.qubit1_address, 0);
    EXPECT_EQ(gate.angle, Angle_t::_MINUS_PI);
    EXPECT_EQ(gate.axis, Axis_t::Z_AXIS);
    EXPECT_EQ(frame[3], crc8_ccitt(frame, 3));

    ASSERT_EQ(handler.receive_data(frame, 4), 0);
    QubitMeasure measure(frame);
    EXPECT_EQ(measure.command_type, Command_t::QUBIT_MEASUREMENT);
    EXPECT_EQ(measure.qubit1_address, 0);
    EXPECT_EQ(measure.basis, 0);
    EXPECT_EQ(frame[3], crc8_ccitt(frame, 3));

    // Fourth call wraps back to the first command in the sequence.
    ASSERT_EQ(handler.receive_data(frame, 4), 0);
    QubitInitialize wrapped(frame);
    EXPECT_EQ(wrapped.command_type, Command_t::QUBIT_INIT);
}

TEST(MockHandlerTest, ReceiveDataRejectsSizeLargerThanFourWithoutAdvancingCycle) {
    MockHandler handler;
    uint8_t oversized[8] = {0};
    EXPECT_EQ(handler.receive_data(oversized, 5), -1);

    // The rejected call must not have consumed a cycle position: the next valid
    // call should still yield the FIRST command (InitQubit), not the second.
    uint8_t frame[4];
    ASSERT_EQ(handler.receive_data(frame, 4), 0);
    QubitInitialize init(frame);
    EXPECT_EQ(init.command_type, Command_t::QUBIT_INIT);
}

TEST(MockHandlerTest, SendDataAlwaysReturnsMinusOne) {
    MockHandler handler;
    uint8_t small[4] = {0, 0, 0, 0};
    EXPECT_EQ(handler.send_data(small, 4), -1);

    uint8_t oversized[8] = {0};
    EXPECT_EQ(handler.send_data(oversized, 8), -1);
}

TEST(MockHandlerTest, TransferFillsRxBufferForEachFourByteSlotAndReturnsZero) {
    MockHandler handler;
    uint8_t tx[8] = {0}; // unused by the implementation, but must be a valid pointer
    uint8_t rx[8] = {0};

    ASSERT_EQ(handler.transfer(tx, rx, 8), 0);

    QubitInitialize first(rx);
    EXPECT_EQ(first.command_type, Command_t::QUBIT_INIT);
    SingleQubitGate second(rx + 4);
    EXPECT_EQ(second.command_type, Command_t::SINGLE_QUBIT_GATE);
}

TEST(MockHandlerTest, TransferWithTwelveBytesProcessesAllThreeCommandsInOneCall) {
    MockHandler handler;
    uint8_t tx[12] = {0};
    uint8_t rx[12] = {0};

    ASSERT_EQ(handler.transfer(tx, rx, 12), 0);

    QubitInitialize first(rx);
    EXPECT_EQ(first.command_type, Command_t::QUBIT_INIT);
    SingleQubitGate second(rx + 4);
    EXPECT_EQ(second.command_type, Command_t::SINGLE_QUBIT_GATE);
    QubitMeasure third(rx + 8);
    EXPECT_EQ(third.command_type, Command_t::QUBIT_MEASUREMENT);
}

TEST(MockHandlerTest, FlushDoesNotCrash) {
    MockHandler handler;
    handler.flush();
    SUCCEED();
}

} // namespace
