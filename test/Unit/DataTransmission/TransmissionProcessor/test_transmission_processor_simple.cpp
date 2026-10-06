// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cstring>

#include "DataTransmission/transmision_handler.hpp"
#include "DataTransmission/TransmissionProcessor/transmission_processor.hpp"
#include "DataTypes/commands.hpp"
#include "DataTypes/responses.hpp"
#include "mocks/mock_transmission_handler.hpp"

using ::testing::_;
using ::testing::Invoke;
using ::testing::InSequence;
using ::testing::Return;

// -------- Helpers (remove if you already have them in a common header) -----
static constexpr uint8_t CRC8_POLY   = 0x07u;
static constexpr uint8_t CRC8_INIT   = 0x00u;
static constexpr uint8_t CRC8_XOROUT = 0x00u;

static uint8_t crc8_ccitt_update(uint8_t crc, uint8_t byte) {
    crc ^= byte;
    for (uint8_t i = 0; i < 8; ++i)
        crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ CRC8_POLY)
                           : static_cast<uint8_t>(crc << 1);
    return crc;
}
static uint8_t crc8_ccitt(const uint8_t *data, size_t len) {
    uint8_t crc = CRC8_INIT;
    while (len--) crc = crc8_ccitt_update(crc, *data++);
    return static_cast<uint8_t>(crc ^ CRC8_XOROUT);
}
static void build_command_frame(uint8_t opcode, uint8_t b1, uint8_t b2, uint8_t out[4]) {
    out[0] = opcode;
    out[1] = b1;
    out[2] = b2;
    out[3] = crc8_ccitt(out, 3);
}

// ==========================================================================
// A) receive_command(...) tests
// ==========================================================================

TEST(TransmissionProcessor_Receive, ReceivesSingleNopCommand) {
    auto handler = std::make_unique<MockTransmissionHandler>();
    auto* h = handler.get();

    uint8_t frame[4];
    build_command_frame(static_cast<uint8_t>(Command_t::NOP), 0x01, 0x02, frame);

    EXPECT_CALL(*h, receive_data(_, 4))
        .WillOnce(Invoke([&](uint8_t* dst, uint8_t len) -> int8_t {
            EXPECT_EQ(len, 4);
            std::memcpy(dst, frame, 4);
            return 0;
        }));

    TransmissionProcessor tp(std::unique_ptr<ITransmissionHandler>(handler.release()));
    std::unique_ptr<CommandBase> cmd;
    EXPECT_EQ(tp.receive_command(cmd, /*timeout_ms=*/100), 0);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->command_type, Command_t::NOP);
}

TEST(TransmissionProcessor_Receive, ReceivesQubitInit) {
    auto handler = std::make_unique<MockTransmissionHandler>();
    auto* h = handler.get();

    uint8_t frame[4];
    build_command_frame(static_cast<uint8_t>(Command_t::QUBIT_INIT), 0x10, 0x20, frame);

    EXPECT_CALL(*h, receive_data(_, 4))
        .WillOnce(Invoke([&](uint8_t* dst, uint8_t len) -> int8_t {
            std::memcpy(dst, frame, 4); return 0;
        }));

    TransmissionProcessor tp(std::unique_ptr<ITransmissionHandler>(handler.release()));
    std::unique_ptr<CommandBase> cmd;
    EXPECT_EQ(tp.receive_command(cmd, 100), 0);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->command_type, Command_t::QUBIT_INIT);
}

// Regression test: deserialize_command's fast bounds check used to compare
// against Command_t::QUBIT_SWAP (0x09) instead of the actual highest
// implemented opcode, Command_t::ENTANGLEMENT_SWAP (0x0A) -- so an
// ENTANGLEMENT_SWAP frame was silently rejected as a decode failure before
// ever reaching the dispatch switch, even though SimulatorManager fully
// implements entanglement_swap_handler. Now fixed in
// transmission_processor.cpp; this pins it down so it can't silently regress.
TEST(TransmissionProcessor_Receive, ReceivesEntanglementSwap) {
    auto handler = std::make_unique<MockTransmissionHandler>();
    auto* h = handler.get();

    uint8_t frame[4];
    build_command_frame(static_cast<uint8_t>(Command_t::ENTANGLEMENT_SWAP), 0x12, 0x00, frame);

    EXPECT_CALL(*h, receive_data(_, 4))
        .WillOnce(Invoke([&](uint8_t* dst, uint8_t len) -> int8_t {
            std::memcpy(dst, frame, 4); return 0;
        }));

    TransmissionProcessor tp(std::unique_ptr<ITransmissionHandler>(handler.release()));
    std::unique_ptr<CommandBase> cmd;
    EXPECT_EQ(tp.receive_command(cmd, 100), 0);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->command_type, Command_t::ENTANGLEMENT_SWAP);
}

TEST(TransmissionProcessor_Receive, UnknownOpcodeReturnsMinus3) {
    auto handler = std::make_unique<MockTransmissionHandler>();
    auto* h = handler.get();

    uint8_t frame[4];
    build_command_frame(0xFF /* unknown */, 0xAA, 0xBB, frame);

    EXPECT_CALL(*h, receive_data(_, 4))
        .WillOnce(Invoke([&](uint8_t* dst, uint8_t len) -> int8_t {
            std::memcpy(dst, frame, 4); return 0;
        }));

    TransmissionProcessor tp(std::unique_ptr<ITransmissionHandler>(handler.release()));
    std::unique_ptr<CommandBase> cmd;
    EXPECT_EQ(tp.receive_command(cmd, 50), -3);
    EXPECT_EQ(cmd, nullptr);
}

TEST(TransmissionProcessor_Receive, BadCrcReturnsMinus3) {
    auto handler = std::make_unique<MockTransmissionHandler>();
    auto* h = handler.get();

    uint8_t frame[4];
    build_command_frame(static_cast<uint8_t>(Command_t::NOP), 0x01, 0x02, frame);
    frame[3] ^= 0x7F; // corrupt CRC

    EXPECT_CALL(*h, receive_data(_, 4))
        .WillOnce(Invoke([&](uint8_t* dst, uint8_t len) -> int8_t {
            std::memcpy(dst, frame, 4); return 0;
        }));

    TransmissionProcessor tp(std::unique_ptr<ITransmissionHandler>(handler.release()));
    std::unique_ptr<CommandBase> cmd;
    EXPECT_EQ(tp.receive_command(cmd, 100), -2);
    EXPECT_EQ(cmd, nullptr);
}

TEST(TransmissionProcessor_Receive, TransportTimeoutOrErrorReturnsMinus2) {
    auto handler = std::make_unique<MockTransmissionHandler>();
    auto* h = handler.get();

    EXPECT_CALL(*h, receive_data(_, 4))
        .WillOnce(Return(int8_t{-1})); // simulate timeout/error

    TransmissionProcessor tp(std::unique_ptr<ITransmissionHandler>(handler.release()));
    std::unique_ptr<CommandBase> cmd;
    EXPECT_EQ(tp.receive_command(cmd, 5), -1);
    EXPECT_EQ(cmd, nullptr);
}

TEST(TransmissionProcessor_Receive, OverwritesExistingCommandPtrOnSuccess) {
    auto handler = std::make_unique<MockTransmissionHandler>();
    auto* h = handler.get();

    // First frame: NOP
    uint8_t f1[4];
    build_command_frame(static_cast<uint8_t>(Command_t::NOP), 0x01, 0x02, f1);
    // Second frame: QUBIT_INIT
    uint8_t f2[4];
    build_command_frame(static_cast<uint8_t>(Command_t::QUBIT_INIT), 0x03, 0x04, f2);

    EXPECT_CALL(*h, receive_data(_, 4))
        .WillOnce(Invoke([&](uint8_t* dst, uint8_t len) -> int8_t { std::memcpy(dst, f1, 4); return 0; }))
        .WillOnce(Invoke([&](uint8_t* dst, uint8_t len) -> int8_t { std::memcpy(dst, f2, 4); return 0; }));

    TransmissionProcessor tp(std::unique_ptr<ITransmissionHandler>(handler.release()));
    std::unique_ptr<CommandBase> cmd;

    EXPECT_EQ(tp.receive_command(cmd, 100), 0);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->command_type, Command_t::NOP);

    // Call again: ptr should be replaced with second command
    EXPECT_EQ(tp.receive_command(cmd, 100), 0);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->command_type, Command_t::QUBIT_INIT);
}

// ==========================================================================
// B) send_response(...) tests
// ==========================================================================

TEST(TransmissionProcessor_Send, SendsSingleResponseSuccessNibblePath) {
    auto handler = std::make_unique<MockTransmissionHandler>();
    auto* h = handler.get();

    auto resp = std::make_unique<ResponseData>();
    resp->status = NodeStatus_t::READY;
    resp->outcome = static_cast<uint8_t>(Outcome_Status_t::SUCCESS) | static_cast<uint8_t>(Bell_State_Measurent_t::BELL_STATE_PHI_MINUS);

    EXPECT_CALL(*h, send_data(_, 4))
        .WillOnce(Invoke([&](const uint8_t* tx, uint8_t len) -> int8_t {
            EXPECT_EQ(len, 4);
            EXPECT_EQ(crc8_ccitt(tx, 3), tx[3]); // CRC valid
            return 0;
        }));

    TransmissionProcessor tp(std::unique_ptr<ITransmissionHandler>(handler.release()));
    EXPECT_EQ(tp.send_response(resp), 0);
}

TEST(TransmissionProcessor_Send, SendsSingleResponseFailureNibblePath) {
    auto handler = std::make_unique<MockTransmissionHandler>();
    auto* h = handler.get();

    auto resp = std::make_unique<ResponseData>();
    resp->status = NodeStatus_t::READY;
    resp->outcome = static_cast<uint8_t>(Outcome_Status_t::UNKNOWN_FAILURE) | static_cast<uint8_t>(Bell_State_Measurent_t::BELL_STATE_PHI_PLUS); // force non-success packing

    EXPECT_CALL(*h, send_data(_, 4))
        .WillOnce(Invoke([&](const uint8_t* tx, uint8_t len) -> int8_t {
            EXPECT_EQ(len, 4);
            EXPECT_EQ(crc8_ccitt(tx, 3), tx[3]);
            return 0;
        }));

    TransmissionProcessor tp(std::unique_ptr<ITransmissionHandler>(handler.release()));
    EXPECT_EQ(tp.send_response(resp), 0);
}

TEST(TransmissionProcessor_Send, NullResponseReturnsMinus1) {
    auto handler = std::make_unique<MockTransmissionHandler>();
    TransmissionProcessor tp(std::unique_ptr<ITransmissionHandler>(handler.release()));
    const std::unique_ptr<ResponseData> null_resp;
    EXPECT_EQ(tp.send_response(null_resp), -1);
}

TEST(TransmissionProcessor_Send, TransportWriteErrorReturnsMinus2) {
    auto handler = std::make_unique<MockTransmissionHandler>();
    auto* h = handler.get();

    auto resp = std::make_unique<ResponseData>();
    resp->status = NodeStatus_t::READY;
    resp->outcome = static_cast<uint8_t>(Outcome_Status_t::SUCCESS) | static_cast<uint8_t>(Bell_State_Measurent_t::BELL_STATE_PSI_PLUS);

    EXPECT_CALL(*h, send_data(_, 4))
        .WillOnce(Return(int8_t{-1}));

    TransmissionProcessor tp(std::unique_ptr<ITransmissionHandler>(handler.release()));
    EXPECT_EQ(tp.send_response(resp), -2);
}

// ==========================================================================
// C) Sequencing tests (send then receive; and multiple sends)
// ==========================================================================

TEST(TransmissionProcessor_Sequence, SendThenReceiveInOrder) {
    auto handler = std::make_unique<MockTransmissionHandler>();
    auto* h = handler.get();
    InSequence seq;

    // Expect send first
    EXPECT_CALL(*h, send_data(_, 4))
        .WillOnce(Invoke([&](const uint8_t* tx, uint8_t len) -> int8_t {
            EXPECT_EQ(len, 4);
            EXPECT_EQ(crc8_ccitt(tx, 3), tx[3]); return 0;
        }));

    // Then receive one command
    uint8_t frame[4];
    build_command_frame(static_cast<uint8_t>(Command_t::NOP), 0xAA, 0xBB, frame);
    EXPECT_CALL(*h, receive_data(_, 4))
        .WillOnce(Invoke([&](uint8_t* dst, uint8_t len) -> int8_t {
            std::memcpy(dst, frame, 4); return 0;
        }));

    TransmissionProcessor tp(std::unique_ptr<ITransmissionHandler>(handler.release()));

    auto resp = std::make_unique<ResponseData>();
    resp->status = NodeStatus_t::READY;
    resp->outcome = static_cast<uint8_t>(Outcome_Status_t::SUCCESS) | static_cast<uint8_t>(Bell_State_Measurent_t::BELL_STATE_PHI_MINUS);
    EXPECT_EQ(tp.send_response(resp), 0);

    std::unique_ptr<CommandBase> cmd;
    EXPECT_EQ(tp.receive_command(cmd, 50), 0);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->command_type, Command_t::NOP);
}

TEST(TransmissionProcessor_Sequence, MultipleSendsBackToBack) {
    auto handler = std::make_unique<MockTransmissionHandler>();
    auto* h = handler.get();

    EXPECT_CALL(*h, send_data(_, 4))
        .Times(3)
        .WillRepeatedly(Invoke([&](const uint8_t *tx, uint8_t len) -> int8_t
                               {
            EXPECT_EQ(len, 4);
            EXPECT_EQ(crc8_ccitt(tx, 3), tx[3]); return 0; }));

    TransmissionProcessor tp(std::unique_ptr<ITransmissionHandler>(handler.release()));

    for (int i = 0; i < 3; ++i)
    {
        auto resp = std::make_unique<ResponseData>();
        resp->status = NodeStatus_t::READY;
        resp->outcome = (i == 1) ? static_cast<uint8_t>(Outcome_Status_t::UNKNOWN_FAILURE) : 
        static_cast<uint8_t>(Outcome_Status_t::SUCCESS) | 
        static_cast<uint8_t>(Bell_State_Measurent_t::BELL_STATE_PHI_PLUS);

        EXPECT_EQ(tp.send_response(resp), 0);
    }
}

TEST(TransmissionProcessor_Sequence, ReceiveTwiceThenTransportError)
{
    auto handler = std::make_unique<MockTransmissionHandler>();
    auto* h = handler.get();

    uint8_t f1[4], f2[4];
    build_command_frame(static_cast<uint8_t>(Command_t::NOP), 0x01, 0x02, f1);
    build_command_frame(static_cast<uint8_t>(Command_t::QUBIT_INIT), 0x03, 0x04, f2);

    EXPECT_CALL(*h, receive_data(_, 4))
        .WillOnce(Invoke([&](uint8_t* dst, uint8_t len) -> int8_t { std::memcpy(dst, f1, 4); return 0; }))
        .WillOnce(Invoke([&](uint8_t* dst, uint8_t len) -> int8_t { std::memcpy(dst, f2, 4); return 0; }))
        .WillOnce(Return(int8_t{-1}));

    TransmissionProcessor tp(std::unique_ptr<ITransmissionHandler>(handler.release()));
    std::unique_ptr<CommandBase> cmd;

    EXPECT_EQ(tp.receive_command(cmd, 100), 0);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->command_type, Command_t::NOP);

    EXPECT_EQ(tp.receive_command(cmd, 100), 0);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->command_type, Command_t::QUBIT_INIT);

    EXPECT_EQ(tp.receive_command(cmd, 10), -1);
}
