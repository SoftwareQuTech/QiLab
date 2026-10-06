// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "DataTransmission/TransmissionProcessor/transmission_processor.hpp"
#include "DataTransmission/TransmissionRunner/transmission_runner.hpp"
#include "DataTransmission/transmision_handler.hpp"
#include "DataTypes/commands.hpp"
#include "DataTypes/responses.hpp"
#include "MessageBroker/message_broker.hpp"
#include "mocks/mock_transmission_handler.hpp"

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;

// ----------------------------------------------------------------------------------
// Test helpers
// ----------------------------------------------------------------------------------
//
// DataTransmissionRunner::on_run() only ever calls ITransmissionHandler::transfer()
// (via TransmissionProcessor::transmit_msg()) -- send_data()/receive_data()/flush()
// are not on this runner's path, so only transfer() needs behavior configured below.
//
// Frame layout mirrors TransmissionProcessor: byte0=opcode/cycle, byte1=payload,
// byte2=payload, byte3=CRC-8/CCITT over bytes[0..2]. Same helper as used in
// test_transmission_processor_sync.cpp.

static constexpr uint8_t CRC8_POLY = 0x07u;
static constexpr uint8_t CRC8_INIT = 0x00u;
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

// Polls `pred` on the calling thread until it returns true or `timeout` elapses.
// Used instead of a fixed sleep-then-assert since the runner's loop cadence is not
// deterministically controllable from the test.
template <typename Predicate>
static bool wait_until(Predicate pred, std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    do {
        if (pred()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    } while (std::chrono::steady_clock::now() < deadline);
    return pred();
}

// RAII guard that always calls on_abort() + joins the runner thread, even if a
// gtest ASSERT_* triggers an early return from the test body. on_run() blocks
// until aborted, so it must always be driven from a dedicated thread.
class RunnerThreadGuard {
public:
    explicit RunnerThreadGuard(DataTransmissionRunner &runner)
        : runner_(runner), thread_([&runner]() { runner.on_run(-1); }) {}

    ~RunnerThreadGuard() {
        runner_.on_abort();
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    RunnerThreadGuard(const RunnerThreadGuard &) = delete;
    RunnerThreadGuard &operator=(const RunnerThreadGuard &) = delete;

private:
    DataTransmissionRunner &runner_;
    std::thread thread_;
};

// ----------------------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------------------
//
// Direction of travel, confirmed by reading DataTransmissionRunner::on_run():
//   - "responses" already computed by SimRunner/EntanglementRunner are pulled OUT of
//     the broker via message_broker->get_response() (source=SimRunner|EntanglementRunner,
//     destination=CommandRunner) and handed to TransmissionProcessor::transmit_msg(),
//     which serializes them and calls ITransmissionHandler::transfer() to send them
//     out over the transport.
//   - Whatever transfer() simultaneously "receives" (the mock's rx buffer) is decoded
//     into CommandBase objects, which the runner then routes INTO the broker via
//     route_command() with source=CommandRunner and destination=SimRunner or
//     EntanglementRunner (this runner plays the CommandRunner role in MessageRoute
//     terms). NOP / undecodable commands are not routed; the corresponding response
//     slot is just reset to READY/SUCCESS.
//   - TransportData<ResponseData>::node_id is used directly as an index into the
//     runner's internal `responses` vector (size == registered_nodes.size()), NOT as
//     the node's own id -- confirmed by `responses[response.node_id] = ...` and the
//     adjacent LOG_INFO which looks up registered_nodes[response.node_id].

TEST(DataTransmissionRunnerTest, CommandFromTransportIsRoutedAndBrokerResponseIsSerialized) {
    auto broker = std::make_shared<MessageBroker>();

    auto handler = std::make_unique<NiceMock<MockTransmissionHandler>>();
    auto *h = handler.get();

    uint8_t qubit_init_frame[4];
    build_command_frame(static_cast<uint8_t>(Command_t::QUBIT_INIT), 0x03, 0x00, qubit_init_frame);
    uint8_t nop_frame[4];
    build_command_frame(static_cast<uint8_t>(Command_t::NOP), 0x00, 0x00, nop_frame);

    std::atomic<bool> served_command{false};
    std::mutex tx_mutex;
    bool captured_tx_valid = false;
    std::array<uint8_t, 4> captured_tx{};

    ON_CALL(*h, transfer(_, _, _))
        .WillByDefault(Invoke([&](uint8_t const *tx, uint8_t const *rx, const uint8_t len) -> int8_t {
            if (len >= 4) {
                std::lock_guard<std::mutex> lock(tx_mutex);
                if (!captured_tx_valid) {
                    std::memcpy(captured_tx.data(), tx, 4);
                    captured_tx_valid = true;
                }
            }
            auto *rx_mut = const_cast<uint8_t *>(rx);
            if (!served_command.exchange(true)) {
                std::memcpy(rx_mut, qubit_init_frame, 4);
            } else {
                std::memcpy(rx_mut, nop_frame, 4);
            }
            return 0;
        }));

    std::vector<int16_t> nodes{42};
    auto processor = std::make_unique<TransmissionProcessor>(std::unique_ptr<ITransmissionHandler>(handler.release()));
    DataTransmissionRunner runner(nodes, std::move(processor), broker, /*cycle_time_us=*/100.0f);

    // A response already produced by SimRunner, destined for node index 0 (== node 42).
    // Pushed before the runner thread starts, so it is guaranteed visible on cycle 0.
    TransportData<ResponseData> outgoing_response;
    outgoing_response.source = DestinationType::SimRunner;
    outgoing_response.destination = DestinationType::CommandRunner;
    outgoing_response.node_id = 0;
    outgoing_response.transport = std::make_unique<ResponseData>();
    outgoing_response.transport->status = NodeStatus_t::BUSY_CMD;
    outgoing_response.transport->outcome = Outcome_Status_t::SUCCESS;
    ASSERT_TRUE(broker->route_response(std::move(outgoing_response)));

    RunnerThreadGuard guard(runner);

    TransportData<CommandBase> routed_command;
    bool got_command = wait_until(
        [&]() { return broker->get_command(routed_command, DestinationType::SimRunner); });

    ASSERT_TRUE(got_command);
    EXPECT_EQ(routed_command.transport->command_type, Command_t::QUBIT_INIT);
    EXPECT_EQ(routed_command.node_id, 42);
    EXPECT_EQ(routed_command.source, DestinationType::CommandRunner);
    EXPECT_EQ(routed_command.destination, DestinationType::SimRunner);

    bool tx_captured = wait_until([&]() {
        std::lock_guard<std::mutex> lock(tx_mutex);
        return captured_tx_valid;
    });
    ASSERT_TRUE(tx_captured);

    std::lock_guard<std::mutex> lock(tx_mutex);
    // Per TransmissionProcessor::transmit_msg()'s inline frame serialization (the path
    // this runner actually exercises, distinct from the unused-here serialize_response()):
    // hi-nibble=outcome, lo-nibble=status.
    const uint8_t expected_byte1 = static_cast<uint8_t>(
        (0xf0 & (static_cast<uint8_t>(Outcome_Status_t::SUCCESS) << 4)) |
        (0x0f & static_cast<uint8_t>(NodeStatus_t::BUSY_CMD)));
    EXPECT_EQ(captured_tx[1], expected_byte1);
}

TEST(DataTransmissionRunnerTest, MultipleRegisteredNodesAreEachServiced) {
    auto broker = std::make_shared<MessageBroker>();

    auto handler = std::make_unique<NiceMock<MockTransmissionHandler>>();
    auto *h = handler.get();

    uint8_t sim_frame[4];
    build_command_frame(static_cast<uint8_t>(Command_t::SINGLE_QUBIT_GATE), 0x01, 0x00, sim_frame);
    uint8_t ent_frame[4];
    build_command_frame(static_cast<uint8_t>(Command_t::ENTANGLEMENT_ATTEMPT), 0x01, 0x02, ent_frame);
    uint8_t nop_frame[4];
    build_command_frame(static_cast<uint8_t>(Command_t::NOP), 0x00, 0x00, nop_frame);

    std::atomic<bool> served{false};

    ON_CALL(*h, transfer(_, _, _))
        .WillByDefault(Invoke([&](uint8_t const *, uint8_t const *rx, const uint8_t len) -> int8_t {
            auto *rx_mut = const_cast<uint8_t *>(rx);
            if (!served.exchange(true) && len >= 8) {
                std::memcpy(rx_mut, sim_frame, 4);
                std::memcpy(rx_mut + 4, ent_frame, 4);
            } else {
                for (uint8_t off = 0; off + 4 <= len; off += 4) {
                    std::memcpy(rx_mut + off, nop_frame, 4);
                }
            }
            return 0;
        }));

    std::vector<int16_t> nodes{10, 20};
    auto processor = std::make_unique<TransmissionProcessor>(std::unique_ptr<ITransmissionHandler>(handler.release()));
    DataTransmissionRunner runner(nodes, std::move(processor), broker, /*cycle_time_us=*/100.0f);

    RunnerThreadGuard guard(runner);

    TransportData<CommandBase> sim_command;
    bool got_sim = wait_until(
        [&]() { return broker->get_command(sim_command, DestinationType::SimRunner); });
    ASSERT_TRUE(got_sim);
    EXPECT_EQ(sim_command.transport->command_type, Command_t::SINGLE_QUBIT_GATE);
    EXPECT_EQ(sim_command.node_id, 10);

    TransportData<CommandBase> ent_command;
    bool got_ent = wait_until(
        [&]() { return broker->get_command(ent_command, DestinationType::EntanglementRunner); });
    ASSERT_TRUE(got_ent);
    EXPECT_EQ(ent_command.transport->command_type, Command_t::ENTANGLEMENT_ATTEMPT);
    EXPECT_EQ(ent_command.node_id, 20);
}

TEST(DataTransmissionRunnerTest, TransportErrorIsHandledWithoutCrashingAndRunnerRecovers) {
    auto broker = std::make_shared<MessageBroker>();

    auto handler = std::make_unique<NiceMock<MockTransmissionHandler>>();
    auto *h = handler.get();

    uint8_t qubit_init_frame[4];
    build_command_frame(static_cast<uint8_t>(Command_t::QUBIT_INIT), 0x02, 0x00, qubit_init_frame);
    uint8_t nop_frame[4];
    build_command_frame(static_cast<uint8_t>(Command_t::NOP), 0x00, 0x00, nop_frame);

    std::atomic<int> call_count{0};
    std::atomic<bool> served{false};
    constexpr int error_cycles = 5;

    // First `error_cycles` calls simulate a transport-level failure
    // (TransmissionProcessor::transmit_msg -> -3, on_run() logs and `continue`s);
    // afterwards the transport "recovers" and starts returning a real command frame.
    ON_CALL(*h, transfer(_, _, _))
        .WillByDefault(Invoke([&](uint8_t const *, uint8_t const *rx, const uint8_t len) -> int8_t {
            const int n = call_count.fetch_add(1);
            if (n < error_cycles) {
                return -1;
            }
            auto *rx_mut = const_cast<uint8_t *>(rx);
            if (!served.exchange(true)) {
                std::memcpy(rx_mut, qubit_init_frame, 4);
            } else {
                std::memcpy(rx_mut, nop_frame, 4);
            }
            return 0;
        }));

    std::vector<int16_t> nodes{7};
    auto processor = std::make_unique<TransmissionProcessor>(std::unique_ptr<ITransmissionHandler>(handler.release()));
    DataTransmissionRunner runner(nodes, std::move(processor), broker, /*cycle_time_us=*/100.0f);

    RunnerThreadGuard guard(runner);

    TransportData<CommandBase> routed_command;
    bool got_command = wait_until(
        [&]() { return broker->get_command(routed_command, DestinationType::SimRunner); });

    ASSERT_TRUE(got_command);
    EXPECT_EQ(routed_command.transport->command_type, Command_t::QUBIT_INIT);
    EXPECT_EQ(routed_command.node_id, 7);
    // The runner must have survived at least the injected error cycles without crashing.
    EXPECT_GE(call_count.load(), error_cycles);
}

TEST(DataTransmissionRunnerTest, OnAbortStopsLoopPromptly) {
    auto broker = std::make_shared<MessageBroker>();

    auto handler = std::make_unique<NiceMock<MockTransmissionHandler>>();
    auto *h = handler.get();

    uint8_t nop_frame[4];
    build_command_frame(static_cast<uint8_t>(Command_t::NOP), 0x00, 0x00, nop_frame);

    ON_CALL(*h, transfer(_, _, _))
        .WillByDefault(Invoke([&](uint8_t const *, uint8_t const *rx, const uint8_t len) -> int8_t {
            auto *rx_mut = const_cast<uint8_t *>(rx);
            for (uint8_t off = 0; off + 4 <= len; off += 4) {
                std::memcpy(rx_mut + off, nop_frame, 4);
            }
            return 0;
        }));

    std::vector<int16_t> nodes{1};
    auto processor = std::make_unique<TransmissionProcessor>(std::unique_ptr<ITransmissionHandler>(handler.release()));
    DataTransmissionRunner runner(nodes, std::move(processor), broker, /*cycle_time_us=*/100.0f);

    // A promise fulfilled by the runner thread itself right after on_run() returns.
    // Waiting on the corresponding future (rather than thread::join()) lets us bound
    // the wait without ever needing two threads to touch the same std::thread object
    // (join()/detach() on the same std::thread from different places is a data race
    // and calling join() twice, or join() after detach(), is undefined behavior /
    // std::terminate risk) -- exactly one of join()/detach() below is called, by the
    // main test thread only, based on what the future tells us.
    std::promise<void> finished_promise;
    std::future<void> finished_future = finished_promise.get_future();

    std::thread runner_thread([&runner, &finished_promise]() {
        runner.on_run(-1);
        finished_promise.set_value();
    });
    // Let the loop actually run a few cycles before requesting a stop.
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    runner.on_abort();

    const bool stopped_promptly =
        finished_future.wait_for(std::chrono::milliseconds(500)) == std::future_status::ready;

    if (stopped_promptly) {
        // on_run() has already returned (the promise is fulfilled); the thread is
        // finishing up or already finished, so this join() returns essentially
        // immediately.
        runner_thread.join();
    } else {
        // The runner thread is still stuck in its loop; detach rather than block the
        // test process indefinitely (or crash it via ~thread() on a joinable thread).
        // This only happens if on_abort() is broken.
        runner_thread.detach();
    }

    EXPECT_TRUE(stopped_promptly) << "DataTransmissionRunner::on_run() did not return promptly after on_abort()";
}

TEST(DataTransmissionRunnerTest, EmptyNodesVectorDoesNotCrashAndAbortsCleanly) {
    auto broker = std::make_shared<MessageBroker>();

    auto handler = std::make_unique<NiceMock<MockTransmissionHandler>>();
    // With zero registered nodes, transfer_msg_count == 0, and
    // TransmissionProcessor::transmit_msg() rejects that (ASSERT_INPUT_RANGE on
    // transfer_msg_count against responses.size()==0) before ever touching the
    // transport handler -- so transfer() must never be invoked in this scenario.
    EXPECT_CALL(*handler, transfer(_, _, _)).Times(0);

    std::vector<int16_t> nodes; // empty on purpose
    auto processor = std::make_unique<TransmissionProcessor>(std::unique_ptr<ITransmissionHandler>(handler.release()));
    DataTransmissionRunner runner(nodes, std::move(processor), broker, /*cycle_time_us=*/100.0f);

    // Just needs to not crash and to abort/join cleanly via the RAII guard.
    RunnerThreadGuard guard(runner);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    SUCCEED();
}
