#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>

#include "DataTypes/commands.hpp"
#include "DataTypes/responses.hpp"
#include "MessageBroker/message_broker.hpp"
#include "Simulator/NoiseModels/EntanglementGenerationNoise/entanglement_attempt_noise.hpp"
#include "Simulator/SimulatorRunners/entanglement_runner.hpp"
#include "Simulator/entanglement_attempt_noise_interface.hpp"
#include "Simulator/link_key.hpp"
#include "mocks/command_mock.hpp"

// EntanglementRunner has no mockable seams by design (its EntanglementManager
// is constructed internally) -- per project decision these tests exercise it
// with its REAL collaborators: a real MessageBroker and the real
// EntanglementManager it builds from the attempt_models map passed to its
// constructor. No new mock interfaces are introduced here.

using AttemptModelsMap = std::unordered_map<uint16_t, std::unique_ptr<EntanglementAttemptNoiseInterface>>;

namespace
{

// EntanglementRunner::on_run() blocks until on_abort() is called, so it must
// always run on its own thread. This guard makes sure on_abort() + join()
// happen even if a mid-test ASSERT_*/FATAL failure does an early `return` --
// without it, a failed assertion would leak a running thread and hang the
// rest of the test binary (join() on the real thread still happens in the
// destructor unwind).
class RunnerThreadGuard
{
public:
    RunnerThreadGuard(EntanglementRunner &runner, std::thread thread)
        : runner_(runner), thread_(std::move(thread))
    {
    }

    ~RunnerThreadGuard()
    {
        runner_.on_abort();
        if (thread_.joinable())
        {
            thread_.join();
        }
    }

    RunnerThreadGuard(const RunnerThreadGuard &) = delete;
    RunnerThreadGuard &operator=(const RunnerThreadGuard &) = delete;

private:
    EntanglementRunner &runner_;
    std::thread thread_;
};

// EntanglementAttempt::nghbr / EntangleMeasure::nghbr decode as
// `((NGHBR_BIT_MASK & data[1]) >> 4) - 1` (see DataTypes/commands.hpp,
// "Adjusting for 0-based indexing"), so to make EntanglementRunner::on_run()
// decode a given `neighbour` id out of the command, the command must be
// *encoded* with neighbour + 1. This only round-trips correctly for
// neighbour <= 14 (the encoded value must fit the 4-bit NGHBR_BIT_MASK
// nibble) -- fine for these tests, which all use small node ids.
std::unique_ptr<CommandBase> make_entanglement_attempt_cmd(uint8_t neighbour)
{
    Entangle cmd(/*fidelity=*/1, static_cast<uint8_t>(neighbour + 1), /*qid=*/0);
    uint8_t data_buffer[4] = {0};
    cmd.create_command(data_buffer, sizeof(data_buffer));
    return std::make_unique<EntanglementAttempt>(data_buffer);
}

std::unique_ptr<CommandBase> make_entanglement_measure_cmd(uint8_t neighbour)
{
    EntanMeasure cmd(/*fidelity=*/1, static_cast<uint8_t>(neighbour + 1), /*qid=*/0,
                      /*try_now=*/false, /*basis=*/false, /*repeat=*/0);
    uint8_t data_buffer[4] = {0};
    cmd.create_command(data_buffer, sizeof(data_buffer));
    return std::make_unique<EntangleMeasure>(data_buffer);
}

// Builds a guaranteed-success (p_success=1.0, 1 attempt), short-duration
// attempt model for a single link -- mirrors
// test/Functional/Entanglement/test_entanglement_manager.cpp's
// configure_permissive_link helper, but returns the map directly since
// EntanglementRunner only accepts attempt models via its constructor (no
// configure_links() exposed post-construction).
AttemptModelsMap make_permissive_link(uint8_t node_a, uint8_t node_b, uint64_t duration_ns = 100)
{
    AttemptModelsMap models;
    const uint16_t key = encode_link_key(node_a, node_b);
    models[key] = std::make_unique<EntanglementAttemptModel>(duration_ns, /*batch_size=*/1, /*p_success=*/1.0f,
                                                              /*rng_seed=*/1);
    return models;
}

bool push_command(MessageBroker &broker, uint16_t node_id, std::unique_ptr<CommandBase> cmd)
{
    TransportData<CommandBase> transport;
    transport.source = DestinationType::CommandRunner;
    transport.destination = DestinationType::EntanglementRunner;
    transport.node_id = node_id;
    transport.transport = std::move(cmd);
    return broker.route_command(std::move(transport));
}

// Bounded poll for a command forwarded to SimRunner (a successful,
// matched-and-executed entanglement produces one of these per node, via
// EntanglementRunner::send_command_async). Polling rather than a fixed sleep
// keeps the test fast in the common case while tolerating real scheduling
// jitter from the runner's background thread.
bool wait_for_sim_command(MessageBroker &broker, TransportData<CommandBase> &out, int max_iterations = 200,
                           int sleep_ms = 5)
{
    for (int i = 0; i < max_iterations; ++i)
    {
        if (broker.get_command(out, DestinationType::SimRunner))
        {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
    }
    return false;
}

// Bounded poll for a failure response destined for CommandRunner (produced
// via EntanglementRunner::send_response_async, for both fail-fast and
// timeout paths).
bool wait_for_response(MessageBroker &broker, TransportData<ResponseData> &out, int max_iterations = 200,
                        int sleep_ms = 5)
{
    for (int i = 0; i < max_iterations; ++i)
    {
        if (broker.get_response(out))
        {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
    }
    return false;
}

} // namespace

// Two matching ENTANGLEMENT_ATTEMPT requests (node 1 <-> node 2), on a link
// configured with a guaranteed-success, short-duration attempt model, both
// eventually produce forwarded commands to SimRunner.
TEST(EntanglementRunnerTest, MatchedAttemptPairForwardsCommandsToSimRunner)
{
    auto broker = std::make_shared<MessageBroker>();
    EntanglementRunner runner(broker, make_permissive_link(1, 2, /*duration_ns=*/100));

    std::thread t([&runner] { runner.on_run(-1); });
    RunnerThreadGuard guard(runner, std::move(t));

    ASSERT_TRUE(push_command(*broker, /*node_id=*/1, make_entanglement_attempt_cmd(/*neighbour=*/2)));
    ASSERT_TRUE(push_command(*broker, /*node_id=*/2, make_entanglement_attempt_cmd(/*neighbour=*/1)));

    TransportData<CommandBase> cmd1;
    TransportData<CommandBase> cmd2;
    ASSERT_TRUE(wait_for_sim_command(*broker, cmd1));
    ASSERT_TRUE(wait_for_sim_command(*broker, cmd2));

    EXPECT_EQ(cmd1.source, DestinationType::EntanglementRunner);
    EXPECT_EQ(cmd1.destination, DestinationType::SimRunner);
    EXPECT_EQ(cmd1.transport->command_type, Command_t::ENTANGLEMENT_ATTEMPT);
    EXPECT_EQ(cmd2.transport->command_type, Command_t::ENTANGLEMENT_ATTEMPT);

    std::vector<uint16_t> node_ids = {cmd1.node_id, cmd2.node_id};
    EXPECT_NE(std::find(node_ids.begin(), node_ids.end(), 1), node_ids.end());
    EXPECT_NE(std::find(node_ids.begin(), node_ids.end(), 2), node_ids.end());
}

// A request on a link with NO configured attempt model fails fast (per
// EntanglementManager's documented fail-fast behavior) as soon as the second
// side matches -- both nodes get a failure response promptly, without
// waiting anywhere near the 50ms pending-request timeout.
TEST(EntanglementRunnerTest, MissingLinkModelFailsFastWithResponseToCommandRunner)
{
    auto broker = std::make_shared<MessageBroker>();
    EntanglementRunner runner(broker, AttemptModelsMap{}); // no links configured at all

    std::thread t([&runner] { runner.on_run(-1); });
    RunnerThreadGuard guard(runner, std::move(t));

    ASSERT_TRUE(push_command(*broker, /*node_id=*/3, make_entanglement_attempt_cmd(/*neighbour=*/4)));
    ASSERT_TRUE(push_command(*broker, /*node_id=*/4, make_entanglement_attempt_cmd(/*neighbour=*/3)));

    // Fail-fast happens synchronously once both sides are matched, so this
    // should resolve almost immediately -- cap well under the 50ms
    // pending-request timeout so this genuinely exercises the fast path.
    TransportData<ResponseData> resp1;
    TransportData<ResponseData> resp2;
    ASSERT_TRUE(wait_for_response(*broker, resp1, /*max_iterations=*/40, /*sleep_ms=*/1));
    ASSERT_TRUE(wait_for_response(*broker, resp2, /*max_iterations=*/40, /*sleep_ms=*/1));

    EXPECT_EQ(resp1.source, DestinationType::EntanglementRunner);
    EXPECT_EQ(resp1.destination, DestinationType::CommandRunner);
    EXPECT_EQ(resp1.transport->outcome, Outcome_Status_t::ENTANGLEMENT_FAILURE);
    EXPECT_EQ(resp2.transport->outcome, Outcome_Status_t::ENTANGLEMENT_FAILURE);

    std::vector<uint16_t> node_ids = {resp1.node_id, resp2.node_id};
    EXPECT_NE(std::find(node_ids.begin(), node_ids.end(), 3), node_ids.end());
    EXPECT_NE(std::find(node_ids.begin(), node_ids.end(), 4), node_ids.end());
}

// An unmatched request (only one side ever sent) eventually times out and
// produces a failure response to CommandRunner. This genuinely waits for the
// manager's real ~50ms ENTANGLEMENT_TIMEOUT window -- kept to this single
// case (not a pattern to repeat) to avoid slowing the suite.
TEST(EntanglementRunnerTest, UnmatchedRequestTimesOutWithResponseToCommandRunner)
{
    auto broker = std::make_shared<MessageBroker>();
    // No link model needed: an unmatched request never reaches the
    // attempt-model / fail-fast path, it just expires on its own.
    EntanglementRunner runner(broker, AttemptModelsMap{});

    std::thread t([&runner] { runner.on_run(-1); });
    RunnerThreadGuard guard(runner, std::move(t));

    ASSERT_TRUE(push_command(*broker, /*node_id=*/5, make_entanglement_attempt_cmd(/*neighbour=*/6)));

    // Bound the wait generously (~1s) above the ~50ms timeout to absorb
    // scheduling jitter from the runner's 1ms poll loop, without waiting
    // indefinitely if something regresses.
    TransportData<ResponseData> resp;
    ASSERT_TRUE(wait_for_response(*broker, resp, /*max_iterations=*/200, /*sleep_ms=*/5));

    EXPECT_EQ(resp.source, DestinationType::EntanglementRunner);
    EXPECT_EQ(resp.destination, DestinationType::CommandRunner);
    EXPECT_EQ(resp.node_id, 5);
    EXPECT_EQ(resp.transport->outcome, Outcome_Status_t::ENTANGLEMENT_SYNC_FAILURE);
}

// on_abort() stops the on_run() loop promptly (well within the 1ms poll
// interval plus scheduling slack), rather than the loop lingering or never
// exiting.
TEST(EntanglementRunnerTest, OnAbortStopsLoopPromptly)
{
    auto broker = std::make_shared<MessageBroker>();
    EntanglementRunner runner(broker, AttemptModelsMap{});

    std::atomic<bool> loop_finished{false};
    std::thread t(
        [&runner, &loop_finished]
        {
            runner.on_run(-1);
            loop_finished = true;
        });
    RunnerThreadGuard guard(runner, std::move(t));

    // Let the loop actually spin a few iterations before aborting it.
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    const auto abort_start = std::chrono::steady_clock::now();
    runner.on_abort();

    bool stopped = false;
    for (int i = 0; i < 200; ++i)
    {
        if (loop_finished.load())
        {
            stopped = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    const auto elapsed = std::chrono::steady_clock::now() - abort_start;

    EXPECT_TRUE(stopped);
    EXPECT_LT(elapsed, std::chrono::milliseconds(200));
}

// ENTANGLEMENT_MEASURE commands are routed the same way as
// ENTANGLEMENT_ATTEMPT ones: two matching measure requests (node 7 <-> node
// 8) on a permissively-configured link both eventually produce forwarded
// commands to SimRunner.
TEST(EntanglementRunnerTest, MatchedMeasureRequestsForwardCommandsToSimRunner)
{
    auto broker = std::make_shared<MessageBroker>();
    EntanglementRunner runner(broker, make_permissive_link(7, 8, /*duration_ns=*/100));

    std::thread t([&runner] { runner.on_run(-1); });
    RunnerThreadGuard guard(runner, std::move(t));

    ASSERT_TRUE(push_command(*broker, /*node_id=*/7, make_entanglement_measure_cmd(/*neighbour=*/8)));
    ASSERT_TRUE(push_command(*broker, /*node_id=*/8, make_entanglement_measure_cmd(/*neighbour=*/7)));

    TransportData<CommandBase> cmd1;
    TransportData<CommandBase> cmd2;
    ASSERT_TRUE(wait_for_sim_command(*broker, cmd1));
    ASSERT_TRUE(wait_for_sim_command(*broker, cmd2));

    EXPECT_EQ(cmd1.transport->command_type, Command_t::ENTANGLEMENT_MEASURE);
    EXPECT_EQ(cmd2.transport->command_type, Command_t::ENTANGLEMENT_MEASURE);

    std::vector<uint16_t> node_ids = {cmd1.node_id, cmd2.node_id};
    EXPECT_NE(std::find(node_ids.begin(), node_ids.end(), 7), node_ids.end());
    EXPECT_NE(std::find(node_ids.begin(), node_ids.end(), 8), node_ids.end());
}
