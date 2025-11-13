#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "DataTransmission/transmision_handler.hpp"
#include "DataTransmission/TransmissionProcessor/transmission_processor.hpp"
#include "DataTypes/commands.hpp"
#include "DataTypes/responses.hpp"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;

// ----------------- Test Helpers -----------------
static constexpr uint8_t CRC8_POLY   = 0x07u;
static constexpr uint8_t CRC8_INIT   = 0x00u;
static constexpr uint8_t CRC8_XOROUT = 0x00u;

static uint8_t crc8_ccitt_update(uint8_t crc, uint8_t byte) {
    crc ^= byte;
    for (uint8_t i = 0; i < 8; ++i) {
        crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ CRC8_POLY)
                           : static_cast<uint8_t>(crc << 1);
    }
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

// ----------------- Mock Transport -----------------
class MockTransmissionHandler : public ITransmissionHandler {
public:
    MOCK_METHOD(int8_t,  send_data,    (const uint8_t* data, uint8_t len), (override));
    MOCK_METHOD(int8_t,  receive_data, (uint8_t* data, uint8_t len),       (override));
    MOCK_METHOD(int8_t,  transfer, (uint8_t const* tx, uint8_t const* rx, const uint8_t len),       (override));
    MOCK_METHOD(void,  flush, (),       (override));
    // If you have timeouts/APIs, add MOCK_METHODs accordingly.
};

// ----------------- Tests -----------------
TEST(TransmissionProcessor_ValidateInputs, ResponseNumberMismatch) {
    // Arrange
    const int num_messages = 3;
    auto handler = std::make_unique<MockTransmissionHandler>();
    MockTransmissionHandler* h = handler.get();

    std::vector<std::unique_ptr<ResponseData>> responses;   

    TransmissionProcessor tp(std::unique_ptr<ITransmissionHandler>(handler.release()));
    std::vector<std::unique_ptr<CommandBase>> commands;

    // Act
    int8_t rc = tp.transmit_msg(responses, commands, num_messages);

    // Assert
    EXPECT_EQ(rc, -1);
}

TEST(TransmissionProcessor_ValidateInputs, CommandNumberMismatch) {
    // Arrange
    const int num_messages = 3;
    auto handler = std::make_unique<MockTransmissionHandler>();
    MockTransmissionHandler* h = handler.get();

    std::vector<std::unique_ptr<ResponseData>> responses;   
    responses.resize(2);

    TransmissionProcessor tp(std::unique_ptr<ITransmissionHandler>(handler.release()));
    std::vector<std::unique_ptr<CommandBase>> commands;

    // Act
    int8_t rc = tp.transmit_msg(responses, commands, num_messages);

    // Assert
    EXPECT_EQ(rc, -1);
}

TEST(TransmissionProcessor_ValidateInputs, CommandResponseNumberMismatch) {
    // Arrange
    const int num_messages = 3;
    auto handler = std::make_unique<MockTransmissionHandler>();
    MockTransmissionHandler* h = handler.get();

    std::vector<std::unique_ptr<ResponseData>> responses;   
    responses.resize(2);

    TransmissionProcessor tp(std::unique_ptr<ITransmissionHandler>(handler.release()));
    std::vector<std::unique_ptr<CommandBase>> commands;
    commands.resize(4);
    // Act
    int8_t rc = tp.transmit_msg(responses, commands, num_messages);

    // Assert
    EXPECT_EQ(rc, -1);
}

TEST(TransmissionProcessor_TransmitMsg, SendsResponsesAndReceivesCommands) {
    // Arrange
    auto handler = std::make_unique<MockTransmissionHandler>();
    MockTransmissionHandler* h = handler.get();

    // 2 responses to send
    auto r1 = std::make_unique<ResponseData>();
    r1->status = NodeStatus_t::READY;
    r1->outcome_status = Outcome_Status_t::SUCCESS;
    r1->outcome_result = Bell_State_Measurent_t::BELL_STATE_PHI_MINUS;

    std::vector<std::unique_ptr<ResponseData>> responses;
    responses.emplace_back(std::make_unique<ResponseData>());
    responses.emplace_back(std::make_unique<ResponseData>());
    responses.emplace_back(std::make_unique<ResponseData>());
    responses.emplace_back(std::make_unique<ResponseData>());
    
    responses[2].swap(r1);

    // Pre-build incoming 2 command frames (e.g., NOP + QUBIT_INIT)
    uint8_t rx_buf[16];
    build_command_frame(static_cast<uint8_t>(Command_t::NOP),        0x12, 0x34, &rx_buf[0]);
    build_command_frame(static_cast<uint8_t>(Command_t::QUBIT_INIT), 0x56, 0x78, &rx_buf[4]);
    build_command_frame(static_cast<uint8_t>(Command_t::NOP),        0x12, 0x34, &rx_buf[8]);
    build_command_frame(static_cast<uint8_t>(Command_t::NOP),        0x12, 0x34, &rx_buf[12]);

    // Expect a single full-duplex transfer of 16 bytes.
    EXPECT_CALL(*h, transfer(::testing::_, ::testing::_, 16))
        .WillOnce(::testing::Invoke([&](uint8_t const* tx, uint8_t const* rx, const uint8_t len) -> int8_t {
            // Validate arguments
            EXPECT_EQ(len, uint8_t{16});
            //ASSERT_NE(tx, nullptr);
            //ASSERT_NE(rx, nullptr);

            // Check each 4B TX frame's CRC (optional sanity)
            for (size_t i = 0; i < len; i += 4) {
                uint8_t crc = crc8_ccitt(tx + i, 3);
                EXPECT_EQ(*(tx + i + 3), crc) << "Bad CRC in TX frame " << (i / 4);
            }

            // Provide RX data: copy prepared frames into the RX buffer
            // NOTE: rx is const in the interface; const_cast to write test data.
            auto *rx_mut = const_cast<uint8_t*>(rx);
            std::memcpy(rx_mut, rx_buf, len);

            return int8_t{0};
        }));

    TransmissionProcessor tp(std::unique_ptr<ITransmissionHandler>(handler.release()));
    std::vector<std::unique_ptr<CommandBase>> commands;
    commands.resize(4);
    // Act
    int8_t rc = tp.transmit_msg(responses, commands, /*transfer_msg_count=*/4);

    // Assert
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(commands.size(), 4u);
    EXPECT_EQ(commands[0]->command_type, Command_t::NOP);
    EXPECT_EQ(commands[1]->command_type, Command_t::QUBIT_INIT);
    EXPECT_EQ(commands[2]->command_type, Command_t::NOP);
    EXPECT_EQ(commands[3]->command_type, Command_t::NOP);
    for (auto &c : commands) {
        EXPECT_NE(c, nullptr);
    }
}

TEST(TransmissionProcessor_TransmitMsg, SendsResponsesAndReceivesEmptyCommands) {
    // Arrange
    auto handler = std::make_unique<MockTransmissionHandler>();
    MockTransmissionHandler* h = handler.get();
    const int num_messages = 3;
    // 2 responses to send
    auto r1 = std::make_unique<ResponseData>();
    r1->status = NodeStatus_t::READY;
    r1->outcome_status = Outcome_Status_t::SUCCESS;
    r1->outcome_result = Bell_State_Measurent_t::BELL_STATE_PHI_MINUS;

    std::vector<std::unique_ptr<ResponseData>> responses;
    responses.emplace_back(std::make_unique<ResponseData>());
    responses.emplace_back(std::make_unique<ResponseData>());
    responses.emplace_back(std::make_unique<ResponseData>());
    
    responses[2].swap(r1);

    // Pre-build incoming 1 command and 2 empty
    uint8_t rx_buf[16] = {0};
    build_command_frame(static_cast<uint8_t>(Command_t::QUBIT_INIT), 0x56, 0x78, &rx_buf[4]);

    // Expect a single full-duplex transfer of 16 bytes.
    EXPECT_CALL(*h, transfer(::testing::_, ::testing::_, num_messages * 4))
        .WillOnce(::testing::Invoke([&](uint8_t const* tx, uint8_t const* rx, const uint8_t len) -> int8_t {
            // Validate arguments
            EXPECT_EQ(len, uint8_t{num_messages * 4});
            //ASSERT_NE(tx, nullptr);
            //ASSERT_NE(rx, nullptr);

            // Check each 4B TX frame's CRC (optional sanity)
            for (size_t i = 0; i < len; i += 4) {
                uint8_t crc = crc8_ccitt(tx + i, 3);
                EXPECT_EQ(*(tx + i + 3), crc) << "Bad CRC in TX frame " << (i / 4);
            }

            // Provide RX data: copy prepared frames into the RX buffer
            // NOTE: rx is const in the interface; const_cast to write test data.
            auto *rx_mut = const_cast<uint8_t*>(rx);
            std::memcpy(rx_mut, rx_buf, len);

            return int8_t{0};
        }));

    TransmissionProcessor tp(std::unique_ptr<ITransmissionHandler>(handler.release()));
    std::vector<std::unique_ptr<CommandBase>> commands;
    commands.resize(num_messages);
    // Act
    int8_t rc = tp.transmit_msg(responses, commands, num_messages);

    // Assert
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(commands.size(), num_messages);
    EXPECT_EQ(commands[0]->command_type, Command_t::NOP);
    EXPECT_EQ(commands[1]->command_type, Command_t::QUBIT_INIT);
    EXPECT_EQ(commands[2]->command_type, Command_t::NOP);
    for (auto &c : commands) {
        EXPECT_NE(c, nullptr);
    }
}

// 1) CRC error in one RX frame -> expect -3 and no decoded commands appended
TEST(TransmissionProcessor_TransmitMsg, FailsOnSingleBadCrcAmongFrames) {
    auto handler = std::make_unique<MockTransmissionHandler>();
    auto* h = handler.get();

    std::vector<std::unique_ptr<ResponseData>> responses(3);
    for (auto& r : responses) {
        r = std::make_unique<ResponseData>();
        r->status = NodeStatus_t::READY;
        r->outcome_status = Outcome_Status_t::SUCCESS;
        r->outcome_result = Bell_State_Measurent_t::BELL_STATE_PHI_PLUS;
    }

    uint8_t rx_buf[12];
    build_command_frame(static_cast<uint8_t>(Command_t::NOP),        0x11, 0x22, &rx_buf[0]);
    build_command_frame(static_cast<uint8_t>(Command_t::QUBIT_INIT), 0x33, 0x44, &rx_buf[4]);
    build_command_frame(static_cast<uint8_t>(Command_t::NOP),        0x55, 0x66, &rx_buf[8]);
    rx_buf[7] ^= 0xFF; // corrupt CRC of 2nd frame

    EXPECT_CALL(*h, transfer(::testing::_, ::testing::_, 12))
        .WillOnce(::testing::Invoke([&](uint8_t const* tx, uint8_t const* rx, const uint8_t len) -> int8_t {
            auto* rx_mut = const_cast<uint8_t*>(rx);
            std::memcpy(rx_mut, rx_buf, len);
            return 0;
        }));

    TransmissionProcessor tp(std::unique_ptr<ITransmissionHandler>(handler.release()));
    std::vector<std::unique_ptr<CommandBase>> commands;
    commands.resize(3);

    int8_t rc = tp.transmit_msg(responses, commands, 3);
    EXPECT_EQ(rc, -4);
}

// 2) Unknown opcode -> expect -3
TEST(TransmissionProcessor_TransmitMsg, FailsOnUnknownOpcode) {
    auto handler = std::make_unique<MockTransmissionHandler>();
    auto* h = handler.get();

    std::vector<std::unique_ptr<ResponseData>> responses(1);
    responses[0] = std::make_unique<ResponseData>();
    responses[0]->status = NodeStatus_t::READY;
    responses[0]->outcome_status = Outcome_Status_t::SUCCESS;
    responses[0]->outcome_result = Bell_State_Measurent_t::BELL_STATE_PHI_MINUS;

    uint8_t rx_buf[4];
    // opcode 0xFF assumed unknown for your Command_t
    build_command_frame(0xFF, 0xAA, 0xBB, rx_buf);

    EXPECT_CALL(*h, transfer(::testing::_, ::testing::_, 4))
        .WillOnce(::testing::Invoke([&](uint8_t const* tx, uint8_t const* rx, const uint8_t len) -> int8_t {
            std::memcpy(const_cast<uint8_t*>(rx), rx_buf, len);
            return 0;
        }));

    TransmissionProcessor tp(std::unique_ptr<ITransmissionHandler>(handler.release()));
    std::vector<std::unique_ptr<CommandBase>> commands(1);

    int8_t rc = tp.transmit_msg(responses, commands, 1);
    EXPECT_EQ(rc, -4);
}

// 3) Null response entry within transfer_msg_count -> expect -1
TEST(TransmissionProcessor_TransmitMsg, RejectsNullResponseWithinCount) {
    auto handler = std::make_unique<MockTransmissionHandler>();

    std::vector<std::unique_ptr<ResponseData>> responses(3);
    responses[0] = std::make_unique<ResponseData>();
    responses[2] = std::make_unique<ResponseData>();
    // responses[1] remains nullptr -> invalid

    TransmissionProcessor tp(std::unique_ptr<ITransmissionHandler>(handler.release()));
    std::vector<std::unique_ptr<CommandBase>> commands(3);
    int8_t rc = tp.transmit_msg(responses, commands, 3);
    EXPECT_EQ(rc, -2);
}

// 4) transfer_msg_count == 0 -> expect -1
TEST(TransmissionProcessor_TransmitMsg, RejectsZeroTransferCount) {
    auto handler = std::make_unique<MockTransmissionHandler>();
    std::vector<std::unique_ptr<ResponseData>> responses(1);
    responses[0] = std::make_unique<ResponseData>();
    TransmissionProcessor tp(std::unique_ptr<ITransmissionHandler>(handler.release()));
    std::vector<std::unique_ptr<CommandBase>> commands;
    int8_t rc = tp.transmit_msg(responses, commands, 0);
    EXPECT_EQ(rc, -1);
}

// 5) transfer_msg_count > responses.size() -> expect -1
TEST(TransmissionProcessor_TransmitMsg, RejectsCountGreaterThanResponses) {
    auto handler = std::make_unique<MockTransmissionHandler>();
    std::vector<std::unique_ptr<ResponseData>> responses(2);
    for (auto& r : responses) r = std::make_unique<ResponseData>();
    TransmissionProcessor tp(std::unique_ptr<ITransmissionHandler>(handler.release()));
    std::vector<std::unique_ptr<CommandBase>> commands(3);
    int8_t rc = tp.transmit_msg(responses, commands, 3);
    EXPECT_EQ(rc, -1);
}

// 6) Transport transfer error -> expect -2
TEST(TransmissionProcessor_TransmitMsg, TransportReturnsError) {
    auto handler = std::make_unique<MockTransmissionHandler>();
    auto* h = handler.get();

    std::vector<std::unique_ptr<ResponseData>> responses(2);
    for (auto& r : responses) {
        r = std::make_unique<ResponseData>();
        r->status = NodeStatus_t::READY;
        r->outcome_status = Outcome_Status_t::SUCCESS;
        r->outcome_result = Bell_State_Measurent_t::BELL_STATE_PHI_PLUS;
    }

    EXPECT_CALL(*h, transfer(::testing::_, ::testing::_, 8))
        .WillOnce(::testing::Return(int8_t{-1}));

    TransmissionProcessor tp(std::unique_ptr<ITransmissionHandler>(handler.release()));
    std::vector<std::unique_ptr<CommandBase>> commands(2);
    int8_t rc = tp.transmit_msg(responses, commands, 2);
    EXPECT_EQ(rc, -3);
}

// 7) Mixed valid + empty RX frames (zeros) -> decode NOP for zeros (assuming opcode 0 == NOP)
TEST(TransmissionProcessor_TransmitMsg, MixedValidAndZeroFramesDecodeAsExpected) {
    auto handler = std::make_unique<MockTransmissionHandler>();
    auto* h = handler.get();

    std::vector<std::unique_ptr<ResponseData>> responses(3);
    for (auto& r : responses) {
        r = std::make_unique<ResponseData>();
        r->status = NodeStatus_t::READY;
        r->outcome_status = Outcome_Status_t::SUCCESS;
        r->outcome_result = Bell_State_Measurent_t::BELL_STATE_PSI_MINUS;
    }

    uint8_t rx_buf[12] = {0}; // all zeros => CRC also 0 with this poly/init/xorout
    build_command_frame(static_cast<uint8_t>(Command_t::QUBIT_INIT), 0x10, 0x20, &rx_buf[4]); // middle frame valid

    EXPECT_CALL(*h, transfer(::testing::_, ::testing::_, 12))
        .WillOnce(::testing::Invoke([&](uint8_t const* tx, uint8_t const* rx, const uint8_t len) -> int8_t {
            std::memcpy(const_cast<uint8_t*>(rx), rx_buf, len);
            return 0;
        }));

    TransmissionProcessor tp(std::unique_ptr<ITransmissionHandler>(handler.release()));
    std::vector<std::unique_ptr<CommandBase>> commands(3);

    int8_t rc = tp.transmit_msg(responses, commands, 3);
    EXPECT_EQ(rc, 0);
    ASSERT_EQ(commands.size(), 3u);
    EXPECT_EQ(commands[0]->command_type, Command_t::NOP);
    EXPECT_EQ(commands[1]->command_type, Command_t::QUBIT_INIT);
    EXPECT_EQ(commands[2]->command_type, Command_t::NOP);
}

// 8) Non-success outcome_status path in serialize (can’t decode nibble here; just ensure CRC ok and RX decodes)
TEST(TransmissionProcessor_TransmitMsg, NonSuccessOutcomeStatusStillTransmitsAndDecodesRx) {
    auto handler = std::make_unique<MockTransmissionHandler>();
    auto* h = handler.get();

    std::vector<std::unique_ptr<ResponseData>> responses(1);
    responses[0] = std::make_unique<ResponseData>();
    responses[0]->status = NodeStatus_t::READY;
    responses[0]->outcome_status = Outcome_Status_t::UNKNOWN_FAILURE; // force non-success nibble path
    responses[0]->outcome_result = Bell_State_Measurent_t::BELL_STATE_PHI_PLUS;

    uint8_t rx_buf[4];
    build_command_frame(static_cast<uint8_t>(Command_t::NOP), 0x01, 0x02, rx_buf);

    EXPECT_CALL(*h, transfer(::testing::_, ::testing::_, 4))
        .WillOnce(::testing::Invoke([&](uint8_t const* tx, uint8_t const* rx, const uint8_t len) -> int8_t {
            // Check TX CRC OK
            EXPECT_EQ(crc8_ccitt(tx, 3), *(tx + 3));
            std::memcpy(const_cast<uint8_t*>(rx), rx_buf, len);
            return 0;
        }));

    TransmissionProcessor tp(std::unique_ptr<ITransmissionHandler>(handler.release()));
    std::vector<std::unique_ptr<CommandBase>> commands(1);
    int8_t rc = tp.transmit_msg(responses, commands, 1);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(commands[0]->command_type, Command_t::NOP);
}

// 9) transfer_msg_count < responses.size(): Only first K are used; rest ignored
TEST(TransmissionProcessor_TransmitMsg, UsesOnlyFirstKResponsesWhenCountLessThanSize) {
    auto handler = std::make_unique<MockTransmissionHandler>();
    auto* h = handler.get();

    std::vector<std::unique_ptr<ResponseData>> responses(4);
    for (auto& r : responses) {
        r = std::make_unique<ResponseData>();
        r->status = NodeStatus_t::READY;
        r->outcome_status = Outcome_Status_t::SUCCESS;
        r->outcome_result = Bell_State_Measurent_t::BELL_STATE_PHI_MINUS;
    }
    const uint8_t k = 4;

    uint8_t rx_buf[16];
    build_command_frame(static_cast<uint8_t>(Command_t::QUBIT_INIT), 0x10, 0x20, &rx_buf[0]);
    build_command_frame(static_cast<uint8_t>(Command_t::NOP),        0x30, 0x40, &rx_buf[4]);
    build_command_frame(static_cast<uint8_t>(Command_t::NOP),        0x30, 0x40, &rx_buf[8]);
    build_command_frame(static_cast<uint8_t>(Command_t::NOP),        0x30, 0x40, &rx_buf[12]);

    EXPECT_CALL(*h, transfer(::testing::_, ::testing::_, k * 4))
        .WillOnce(::testing::Invoke([&](uint8_t const* tx, uint8_t const* rx, const uint8_t len) -> int8_t {
            EXPECT_EQ(len, k * 4);
            std::memcpy(const_cast<uint8_t*>(rx), rx_buf, len);
            return 0;
        }));

    TransmissionProcessor tp(std::unique_ptr<ITransmissionHandler>(handler.release()));
    std::vector<std::unique_ptr<CommandBase>> commands(k);
    int8_t rc = tp.transmit_msg(responses, commands, k);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(commands.size(), k);
    EXPECT_EQ(commands[0]->command_type, Command_t::QUBIT_INIT);
    EXPECT_EQ(commands[1]->command_type, Command_t::NOP);
}

// 10) Maximum batch (8 frames) happy path
TEST(TransmissionProcessor_TransmitMsg, MaxBatchEightFrames) {
    auto handler = std::make_unique<MockTransmissionHandler>();
    auto* h = handler.get();

    constexpr uint8_t N = 8;
    std::vector<std::unique_ptr<ResponseData>> responses(N);
    for (auto& r : responses) {
        r = std::make_unique<ResponseData>();
        r->status = NodeStatus_t::READY;
        r->outcome_status = Outcome_Status_t::SUCCESS;
        r->outcome_result = Bell_State_Measurent_t::BELL_STATE_PSI_PLUS;
    }

    uint8_t rx_buf[N * 4];
    for (uint8_t i = 0; i < N; ++i) {
        uint8_t opcode = (i % 2 == 0) ? static_cast<uint8_t>(Command_t::NOP)
                                      : static_cast<uint8_t>(Command_t::QUBIT_INIT);
        build_command_frame(opcode, i, 0xA0 + i, &rx_buf[i * 4]);
    }

    EXPECT_CALL(*h, transfer(::testing::_, ::testing::_, N * 4))
        .WillOnce(::testing::Invoke([&](uint8_t const* tx, uint8_t const* rx, const uint8_t len) -> int8_t {
            std::memcpy(const_cast<uint8_t*>(rx), rx_buf, len);
            return 0;
        }));

    TransmissionProcessor tp(std::unique_ptr<ITransmissionHandler>(handler.release()));
    std::vector<std::unique_ptr<CommandBase>> commands(N);
    int8_t rc = tp.transmit_msg(responses, commands, N);
    EXPECT_EQ(rc, 0);
    for (uint8_t i = 0; i < N; ++i) {
        ASSERT_NE(commands[i], nullptr);
        if (i % 2 == 0)
            EXPECT_EQ(commands[i]->command_type, Command_t::NOP);
        else
            EXPECT_EQ(commands[i]->command_type, Command_t::QUBIT_INIT);
    }
}

// 11) Empty commands vector passed in (size 0); function should return error. We want preallocated space.
TEST(TransmissionProcessor_TransmitMsg, WorksWithInitiallyEmptyCommandsVector) {
    auto handler = std::make_unique<MockTransmissionHandler>();
    auto* h = handler.get();

    std::vector<std::unique_ptr<ResponseData>> responses(2);
    for (auto& r : responses) {
        r = std::make_unique<ResponseData>();
        r->status = NodeStatus_t::READY;
        r->outcome_status = Outcome_Status_t::SUCCESS;
        r->outcome_result = Bell_State_Measurent_t::BELL_STATE_PHI_PLUS;
    }

    uint8_t rx_buf[8];
    build_command_frame(static_cast<uint8_t>(Command_t::NOP),        0xAA, 0xBB, &rx_buf[0]);
    build_command_frame(static_cast<uint8_t>(Command_t::QUBIT_INIT), 0xCC, 0xDD, &rx_buf[4]);

    TransmissionProcessor tp(std::unique_ptr<ITransmissionHandler>(handler.release()));
    std::vector<std::unique_ptr<CommandBase>> commands; // empty
    int8_t rc = tp.transmit_msg(responses, commands, 2);

    EXPECT_EQ(rc, -1);
    ASSERT_EQ(commands.size(), 0u);
}

// 12) Responses vector contains nullptrs beyond transfer_msg_count -> should be OK
TEST(TransmissionProcessor_TransmitMsg, IgnoresNullResponsesBeyondCount) {
    auto handler = std::make_unique<MockTransmissionHandler>();
    auto* h = handler.get();

    std::vector<std::unique_ptr<ResponseData>> responses(4);
    for (int i = 0; i < 2; ++i) {
        responses[i] = std::make_unique<ResponseData>();
        responses[i]->status = NodeStatus_t::READY;
        responses[i]->outcome_status = Outcome_Status_t::SUCCESS;
        responses[i]->outcome_result = Bell_State_Measurent_t::BELL_STATE_PHI_MINUS;
    }
    // responses[2], responses[3] are nullptr, but count==2 -> OK

    uint8_t rx_buf[8];
    build_command_frame(static_cast<uint8_t>(Command_t::QUBIT_INIT), 0x01, 0x02, &rx_buf[0]);
    build_command_frame(static_cast<uint8_t>(Command_t::NOP),        0x03, 0x04, &rx_buf[4]);

    EXPECT_CALL(*h, transfer(::testing::_, ::testing::_, 2 * 4))
        .WillOnce(::testing::Invoke([&](uint8_t const* tx, uint8_t const* rx, const uint8_t len) -> int8_t {
            std::memcpy(const_cast<uint8_t*>(rx), rx_buf, len);
            return 0;
        }));

    TransmissionProcessor tp(std::unique_ptr<ITransmissionHandler>(handler.release()));
    std::vector<std::unique_ptr<CommandBase>> commands(4); //sizes should match
    int8_t rc = tp.transmit_msg(responses, commands, 2);

    EXPECT_EQ(rc, 0);
    EXPECT_EQ(commands[0]->command_type, Command_t::QUBIT_INIT);
    EXPECT_EQ(commands[1]->command_type, Command_t::NOP);
}
