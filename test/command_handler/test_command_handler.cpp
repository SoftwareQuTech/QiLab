#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <vector>
#include <cstring>

#include "mocks/mock_transmission.hpp"
#include "CommandHandler/command_handler.hpp"
#include "DataTypes/commands.hpp"     // Command_t, Angle_t, Axis_t, CommandBase, masks

using ::testing::_;
using ::testing::Invoke;
using ::testing::NotNull;

//
// Bring in your helper-generators
//
#include "CommandHandler/handlers/command_helper.hpp"

// --- Small local CRC helper (must match BaseC) -------------------------------
static uint8_t crc8_ccitt_update(uint8_t crc, uint8_t byte) noexcept {
    crc ^= byte;
    for (uint8_t i = 0; i < 8; ++i) {
        crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x07u)
                           : static_cast<uint8_t>(crc << 1);
    }
    return crc;
}
static uint8_t crc8_ccitt(const uint8_t *data, std::size_t len) noexcept {
    uint8_t crc = 0x00u;
    while (len--) crc = crc8_ccitt_update(crc, *data++);
    return static_cast<uint8_t>(crc ^ 0x00u);
}

// --- Utility to build a 4-byte frame from a BaseC command --------------------
static std::array<uint8_t, 4> build_frame_from(const BaseC& c) {
    std::array<uint8_t, 4> buf{};
    const auto n = c.create_command(buf.data(), buf.size());
    // All helpers write exactly 4 bytes on success.
    EXPECT_EQ(n, 4u) << "Helper did not emit a 4-byte frame.";
    // Sanity: CRC is correct for first 3 bytes.
    EXPECT_EQ(buf[3], crc8_ccitt(buf.data(), 3));
    return buf;
}

// === Param table of valid commands ==========================================
struct CmdFactory {
    const char* name;
    std::function<std::unique_ptr<BaseC>()> make;
};

static std::vector<CmdFactory> valid_cmds() {
    return {
        {"NOP", []{ return std::make_unique<Nop>(); }},
        {"QUBIT_INIT_0", []{ return std::make_unique<InitQubit>(0); }},
        {"QUBIT_INIT_7", []{ return std::make_unique<InitQubit>(7); }},
        {"SINGLE_Q_X_8_16", []{ return std::make_unique<SingleQGate>(0, Angle_t::_8_PI_16, Axis_t::X_AXIS); }},
        {"SINGLE_Q_Y_15_16", []{ return std::make_unique<SingleQGate>(1, Angle_t::_15_PI_16, Axis_t::Y_AXIS); }},
        {"TWO_Q_X_8_16", []{ return std::make_unique<TwoQGate>(0, 1, Angle_t::_8_PI_16, Axis_t::X_AXIS); }},
        {"MEASURE_Z_BASIS", []{ return std::make_unique<QMeasure>(2, /*basis*/true, Axis_t::Z_AXIS); }},
        {"ENTANGLE_f3_n2_q1", []{ return std::make_unique<Entangle>(3, 2, 1); }},
        {"ENTAN_MEAS_mix", []{ return std::make_unique<EntanMeasure>(2, 1, 0, /*now*/true, /*basis*/false, /*repeat*/5); }},
        {"PREMEAS_SEL", []{ return std::make_unique<PreMeasureGateSelect>(Angle_t::_PI_16, Angle_t::_2_PI_16, Angle_t::_3_PI_16, /*target*/true); }},
        {"QSWAP_0_1", []{ return std::make_unique<QSwap>(0, 1); }},
        {"QMOVE_2_0", []{ return std::make_unique<QMove>(2, 0); }},
        {"ENTANGLE_SWAP_1_3", []{ return std::make_unique<EntangleSwap>(1, 3); }},
    };
}

// === The tests ==============================================================

// Each valid frame should be accepted -> status 0 and non-null cmd.
TEST(CommandHandler_Receive_ValidFrames, AcceptsAllHelpers) {
    for (const auto& f : valid_cmds()) {
        SCOPED_TRACE(::testing::Message() << "Command case: " << f.name);

        MockTransmissionHandler tx;
        CommandHandler uut(&tx);

        auto cmd_helper = f.make();
        const auto frame = build_frame_from(*cmd_helper);

        EXPECT_CALL(tx, receive_data(NotNull(), _))
            .WillOnce(Invoke([&](uint8_t* buf, uint8_t len) -> uint8_t {
                EXPECT_GE(len, static_cast<uint8_t>(frame.size()));
                std::memcpy(buf, frame.data(), frame.size());
                return static_cast<uint8_t>(0); // success from transport
            }));

        std::unique_ptr<CommandBase> cmd;
        const uint8_t st = uut.receive_command(cmd);

        EXPECT_EQ(st, 0) << "receive_command should return success for a valid frame";
        ASSERT_NE(cmd, nullptr) << "Valid frame should deserialize into a CommandBase";
        // If you have an accessor, assert type/id here, e.g.:
        // EXPECT_EQ(cmd->type(), Command_t::NOP);
    }
}

// Corrupted CRC should be rejected -> non-zero status and nullptr cmd.
TEST(CommandHandler_Receive_InvalidCRC, RejectsFrame) {
    MockTransmissionHandler tx;
    CommandHandler uut(&tx);

    Nop nop;
    auto frame = build_frame_from(nop);
    frame[3] ^= 0xFF; // flip CRC

    EXPECT_CALL(tx, receive_data(NotNull(), _))
        .WillOnce(Invoke([&](uint8_t* buf, uint8_t len) -> uint8_t {
            EXPECT_GE(len, static_cast<uint8_t>(frame.size()));
            std::memcpy(buf, frame.data(), frame.size());
            return static_cast<uint8_t>(0);
        }));

    std::unique_ptr<CommandBase> cmd;
    const uint8_t st = uut.receive_command(cmd);

    EXPECT_NE(st, 0) << "Parse must fail for bad CRC";
    EXPECT_EQ(cmd, nullptr);
}

// Malformed size (less than 4 bytes) should be rejected.
TEST(CommandHandler_Receive_ShortFrame, RejectsTooShort) {
    MockTransmissionHandler tx;
    CommandHandler uut(&tx);

    // 2-byte junk
    const std::array<uint8_t, 2> junk{0xAA, 0x55};

    EXPECT_CALL(tx, receive_data(NotNull(), _))
        .WillOnce(Invoke([&](uint8_t* buf, uint8_t /*len*/) -> uint8_t {
            std::memcpy(buf, junk.data(), junk.size());
            return static_cast<int8_t>(0);
        }));

    std::unique_ptr<CommandBase> cmd;
    const uint8_t st = uut.receive_command(cmd);

    EXPECT_NE(st, 0);
    EXPECT_EQ(cmd, nullptr);
}

// Propagate transport error codes (receive_data != 0).
TEST(CommandHandler_Receive_TransportError, PropagatesError) {
    MockTransmissionHandler tx;
    CommandHandler uut(&tx);

    EXPECT_CALL(tx, receive_data(NotNull(), _))
        .WillOnce(Invoke([](uint8_t*, uint8_t){ return static_cast<int8_t>(-2); }));

    std::unique_ptr<CommandBase> cmd;
    EXPECT_EQ(uut.receive_command(cmd), -1);
    EXPECT_EQ(cmd, nullptr);
}
