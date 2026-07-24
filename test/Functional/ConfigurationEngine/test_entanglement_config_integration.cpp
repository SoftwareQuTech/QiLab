/*
** Functional tests for the entanglement-link configuration path end to end:
** a real YAML config (with entanglement_links) is loaded via load_config(),
** then resolved into EntanglementManager attempt models and SimulatorManager
** channel models following the EXACT same pattern application.cpp uses
** (name-to-id resolution, encode_link_key, configure_entanglement_links),
** and a real entanglement attempt is driven through both to verify the
** loaded config actually produces the expected simulated behavior.
**
** Parser-level edge cases (missing keys, unknown type, out-of-range fields,
** duplicate/self links) are already covered in
** test/Unit/ConfigurationEngine/test_entanglement_parsers.cpp and
** test/Unit/ConfigurationEngine/test_config_loader.cpp -- this file is only
** about the higher layer: does a VALID loaded config produce correct
** end-to-end simulated behavior.
*/

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>

#include "ConfigurationEngine/config_loader.hpp"
#include "DataTypes/commands.hpp"
#include "DataTypes/responses.hpp"
#include "Simulator/sim_interface.hpp"
#include "Simulator/SimulatorManager/simulator_manager.hpp"
#include "Simulator/EntanglementManager/entanglement_manager.hpp"
#include "Simulator/NoiseModels/EntanglementGenerationNoise/entanglement_attempt_noise.hpp"
#include "Simulator/NoiseModels/EntanglementGenerationNoise/entanglement_channel_noise.hpp"
#include "Simulator/entanglement_attempt_noise_interface.hpp"
#include "Simulator/entanglement_channel_noise_interface.hpp"
#include "Simulator/link_key.hpp"
#include "mocks/command_mock.hpp"

namespace {

// Records single_qubit_noise calls so tests can verify the CONFIGURED
// channel model (not some other one) is what actually gets applied.
class SpyAdapter : public ISimulator {
public:
    struct NoiseCall { uint8_t qubit; float q_x, q_y, q_z; };
    std::vector<NoiseCall> noise_calls;

    bool qubit_initialize(uint8_t) override { return true; }
    bool single_qubit_gate(uint8_t, Angle_t, Axis_t) override { return true; }
    bool single_qubit_noise(uint8_t qubit, float x, float y, float z) override {
        noise_calls.push_back({qubit, x, y, z});
        return true;
    }
    bool two_qubit_gate(uint8_t, uint8_t, Angle_t, Axis_t) override { return true; }
    uint8_t qubit_measure(uint8_t, bool) override { return 0; }
    int8_t attempt_entanglement(uint16_t, uint8_t, uint8_t, bool, uint8_t) override { return 0; } // STATE_PHI_PLUS
    bool qubit_move(uint8_t, uint8_t) override { return true; }
    bool qubit_swap(uint8_t, uint8_t) override { return true; }
    bool entanglement_swap(uint8_t, uint8_t) override { return true; }
    bool allocate_qubits(uint8_t) override { return true; }
};

// EntanglementAttempt::nghbr decodes as ((NGHBR_BIT_MASK & data[1]) >> 4) - 1
// (see DataTypes/commands.hpp, "Adjusting for 0-based indexing"), so the wire
// value must be encoded as neighbour + 1 (only round-trips for neighbour <=
// 14) -- same convention used in test_entanglement_runner.cpp.
std::unique_ptr<CommandBase> make_entanglement_attempt(uint16_t neighbour) {
    Entangle cmd(/*fidelity=*/1, static_cast<uint8_t>(neighbour + 1), /*qid=*/0);
    uint8_t data_buffer[4] = {0};
    cmd.create_command(data_buffer, sizeof(data_buffer));
    return std::make_unique<EntanglementAttempt>(data_buffer);
}

class EntanglementConfigIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        const auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        temp_dir = std::filesystem::temp_directory_path() /
                   ("qilab_ent_config_integration_" + std::to_string(now));
        std::filesystem::create_directories(temp_dir);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(temp_dir, ec);
    }

    std::filesystem::path write_config(const std::string &content) {
        const std::filesystem::path path = temp_dir / "config.yaml";
        std::ofstream out(path);
        out << content;
        out.close();
        return path;
    }

    // Mirrors application.cpp lines ~113-153: build SimulatorManager, register
    // every config->nodes entry, resolve entanglement_links into attempt/channel
    // model maps keyed by encode_link_key via the name->id map established by
    // registration, and call configure_entanglement_links. Returns the attempt
    // models map separately since (like EntanglementRunner) this test drives
    // EntanglementManager directly rather than through a full runner thread.
    struct ResolvedSystem {
        std::unique_ptr<SimulatorManager> sim_manager;
        SpyAdapter *spy = nullptr;
        std::unordered_map<uint16_t, std::unique_ptr<EntanglementAttemptNoiseInterface>> attempt_models;
        std::unordered_map<std::string, uint16_t> name_to_node_id;
    };

    ResolvedSystem resolve_system(const AppConfig &config) {
        ResolvedSystem sys;
        auto spy = std::make_unique<SpyAdapter>();
        sys.spy = spy.get();
        sys.sim_manager = std::make_unique<SimulatorManager>(
            std::move(spy), config.system.qubit_noise_enabled, config.system.entanglement_noise_enabled);

        std::vector<int16_t> node_ids;
        for (const auto &node : config.nodes) {
            int16_t id = sys.sim_manager->register_node(node);
            node_ids.push_back(id);
            sys.name_to_node_id[node.name] = static_cast<uint16_t>(id);
        }

        std::unordered_map<uint16_t, std::unique_ptr<EntanglementChannelNoiseInterface>> channel_models;
        for (const auto &link : config.entanglement_links) {
            const uint16_t id_a = sys.name_to_node_id.at(link.node_a);
            const uint16_t id_b = sys.name_to_node_id.at(link.node_b);
            const uint16_t key = encode_link_key(static_cast<uint8_t>(id_a), static_cast<uint8_t>(id_b));

            if (const auto *basic = std::get_if<EntanglementAttemptBasicConfig>(&link.attempt_model.config_data)) {
                sys.attempt_models[key] = std::make_unique<EntanglementAttemptModel>(
                    basic->attempt_duration_ns, basic->batch_size, basic->p_success, basic->rng_seed);
            }
            if (const auto *basic = std::get_if<EntanglementChannelBasicConfig>(&link.channel_model.config_data)) {
                channel_models[key] = std::make_unique<EntanglementChannelModel>(basic->p_depolarizing, basic->p_dephasing);
            }
        }
        sys.sim_manager->configure_entanglement_links(std::move(channel_models));
        return sys;
    }

    std::filesystem::path temp_dir;
};

// A config with one entanglement_links entry (deterministic-success attempt
// model, real channel model) drives a real matched attempt end to end: both
// sides succeed, and the CONFIGURED channel noise (not some default) is what
// actually gets applied to the initiator's qubit.
TEST_F(EntanglementConfigIntegrationTest, ValidLinkConfigProducesSuccessAndAppliesConfiguredChannelNoise) {
    const std::string yaml =
        "entanglement_links:\n"
        "  - node_a: alice\n"
        "    node_b: bob\n"
        "    attempt_model:\n"
        "      type: BASIC\n"
        "      attempt_duration_ns: 100\n"
        "      batch_size: 1\n"
        "      p_success: 1.0\n"
        "      rng_seed: 1\n"
        "    channel_model:\n"
        "      type: BASIC\n"
        "      p_depolarizing: 0.3\n"
        "      p_dephasing: 0.0\n"
        "nodes:\n"
        "  - name: alice\n"
        "    qubit_arrays:\n"
        "      - num_qubits: 1\n"
        "        noise_model:\n"
        "          type: BASIC\n"
        "          t2_value: 1000\n"
        "  - name: bob\n"
        "    qubit_arrays:\n"
        "      - num_qubits: 1\n"
        "        noise_model:\n"
        "          type: BASIC\n"
        "          t2_value: 1000\n";

    const auto config = load_config(write_config(yaml).string());
    ASSERT_TRUE(config.has_value());

    auto sys = resolve_system(*config);
    const uint16_t alice = sys.name_to_node_id.at("alice");
    const uint16_t bob = sys.name_to_node_id.at("bob");

    std::vector<std::pair<int, Outcome_Status_t>> responses;
    std::vector<std::pair<int, std::unique_ptr<CommandBase>>> commands;
    EntanglementManager ent_manager(
        [&](int node_id, Outcome_Status_t status) { responses.emplace_back(node_id, status); },
        [&](int node_id, std::unique_ptr<CommandBase> cmd) { commands.emplace_back(node_id, std::move(cmd)); });
    ent_manager.configure_links(std::move(sys.attempt_models));

    ent_manager.add_new_request(make_entanglement_attempt(bob), alice, bob);
    ent_manager.add_new_request(make_entanglement_attempt(alice), bob, alice);

    std::this_thread::sleep_for(std::chrono::microseconds(1) + std::chrono::milliseconds(5));
    ent_manager.update_requests();
    ASSERT_EQ(commands.size(), 2u);

    // Drive both forwarded commands through the real SimulatorManager, exactly
    // as SimulatorRunner would after EntanglementRunner forwards them.
    for (auto &[node_id, cmd] : commands) {
        ResponseData response;
        ASSERT_EQ(sys.sim_manager->add_new_command_to_process(node_id, std::move(cmd), response), 0);
        EXPECT_TRUE(response.outcome & static_cast<int>(Outcome_Status_t::SUCCESS));
    }

    // The channel model was only configured for the initiator's side (per
    // entanglement_attempt_handler: "only on the initiator's own qubit"). One
    // of the two nodes is the initiator (node_id < neighbour's id); confirm
    // exactly one noise call landed, with weights matching p_depolarizing=0.3.
    ASSERT_EQ(sys.spy->noise_calls.size(), 1u);
    const auto &call = sys.spy->noise_calls.front();
    EXPECT_NEAR(call.q_x, 0.1f, 1e-6f); // p_depolarizing/3
    EXPECT_NEAR(call.q_y, 0.1f, 1e-6f);
    EXPECT_NEAR(call.q_z, 0.1f, 1e-6f); // p_dephasing=0 contributes nothing extra
}

// A deterministic-failure attempt model (p_success=0.0) resolved from config
// reliably fails the match -- no adapter interaction at all.
TEST_F(EntanglementConfigIntegrationTest, DeterministicFailureConfigFailsReliably) {
    const std::string yaml =
        "entanglement_links:\n"
        "  - node_a: alice\n"
        "    node_b: bob\n"
        "    attempt_model:\n"
        "      type: BASIC\n"
        "      attempt_duration_ns: 100\n"
        "      batch_size: 1\n"
        "      p_success: 0.0\n"
        "      rng_seed: 1\n"
        "    channel_model:\n"
        "      type: BASIC\n"
        "      p_depolarizing: 0.0\n"
        "      p_dephasing: 0.0\n"
        "nodes:\n"
        "  - name: alice\n"
        "    qubit_arrays:\n"
        "      - num_qubits: 1\n"
        "        noise_model:\n"
        "          type: BASIC\n"
        "          t2_value: 1000\n"
        "  - name: bob\n"
        "    qubit_arrays:\n"
        "      - num_qubits: 1\n"
        "        noise_model:\n"
        "          type: BASIC\n"
        "          t2_value: 1000\n";

    const auto config = load_config(write_config(yaml).string());
    ASSERT_TRUE(config.has_value());

    auto sys = resolve_system(*config);
    const uint16_t alice = sys.name_to_node_id.at("alice");
    const uint16_t bob = sys.name_to_node_id.at("bob");

    std::vector<std::pair<int, Outcome_Status_t>> responses;
    EntanglementManager ent_manager(
        [&](int node_id, Outcome_Status_t status) { responses.emplace_back(node_id, status); },
        [&](int, std::unique_ptr<CommandBase>) { FAIL() << "should never succeed"; });
    ent_manager.configure_links(std::move(sys.attempt_models));

    ent_manager.add_new_request(make_entanglement_attempt(bob), alice, bob);
    ent_manager.add_new_request(make_entanglement_attempt(alice), bob, alice);

    ASSERT_EQ(responses.size(), 2u);
    EXPECT_EQ(responses[0].second, Outcome_Status_t::ENTANGLEMENT_FAILURE);
    EXPECT_EQ(responses[1].second, Outcome_Status_t::ENTANGLEMENT_FAILURE);
}

// Two independently-configured links in one config each apply their OWN
// model, not each other's: alice<->bob is permissive, carol<->dave always
// fails -- verifies the resolution loop didn't accidentally cross-wire them.
TEST_F(EntanglementConfigIntegrationTest, TwoLinksInOneConfigUseTheirOwnDistinctModels) {
    const std::string yaml =
        "entanglement_links:\n"
        "  - node_a: alice\n"
        "    node_b: bob\n"
        "    attempt_model:\n"
        "      type: BASIC\n"
        "      attempt_duration_ns: 100\n"
        "      batch_size: 1\n"
        "      p_success: 1.0\n"
        "      rng_seed: 1\n"
        "    channel_model:\n"
        "      type: BASIC\n"
        "      p_depolarizing: 0.0\n"
        "      p_dephasing: 0.0\n"
        "  - node_a: carol\n"
        "    node_b: dave\n"
        "    attempt_model:\n"
        "      type: BASIC\n"
        "      attempt_duration_ns: 100\n"
        "      batch_size: 1\n"
        "      p_success: 0.0\n"
        "      rng_seed: 1\n"
        "    channel_model:\n"
        "      type: BASIC\n"
        "      p_depolarizing: 0.0\n"
        "      p_dephasing: 0.0\n"
        "nodes:\n"
        "  - name: alice\n"
        "    qubit_arrays:\n"
        "      - num_qubits: 1\n"
        "        noise_model:\n"
        "          type: BASIC\n"
        "          t2_value: 1000\n"
        "  - name: bob\n"
        "    qubit_arrays:\n"
        "      - num_qubits: 1\n"
        "        noise_model:\n"
        "          type: BASIC\n"
        "          t2_value: 1000\n"
        "  - name: carol\n"
        "    qubit_arrays:\n"
        "      - num_qubits: 1\n"
        "        noise_model:\n"
        "          type: BASIC\n"
        "          t2_value: 1000\n"
        "  - name: dave\n"
        "    qubit_arrays:\n"
        "      - num_qubits: 1\n"
        "        noise_model:\n"
        "          type: BASIC\n"
        "          t2_value: 1000\n";

    const auto config = load_config(write_config(yaml).string());
    ASSERT_TRUE(config.has_value());
    ASSERT_EQ(config->nodes.size(), 4u);

    auto sys = resolve_system(*config);
    const uint16_t alice = sys.name_to_node_id.at("alice");
    const uint16_t bob = sys.name_to_node_id.at("bob");
    const uint16_t carol = sys.name_to_node_id.at("carol");
    const uint16_t dave = sys.name_to_node_id.at("dave");

    std::vector<std::pair<int, Outcome_Status_t>> responses;
    std::vector<std::pair<int, std::unique_ptr<CommandBase>>> commands;
    EntanglementManager ent_manager(
        [&](int node_id, Outcome_Status_t status) { responses.emplace_back(node_id, status); },
        [&](int node_id, std::unique_ptr<CommandBase> cmd) { commands.emplace_back(node_id, std::move(cmd)); });
    ent_manager.configure_links(std::move(sys.attempt_models));

    ent_manager.add_new_request(make_entanglement_attempt(bob), alice, bob);
    ent_manager.add_new_request(make_entanglement_attempt(alice), bob, alice);
    ent_manager.add_new_request(make_entanglement_attempt(dave), carol, dave);
    ent_manager.add_new_request(make_entanglement_attempt(carol), dave, carol);

    // carol<->dave fails immediately (p_success=0.0, batch_size=1: exhausted on
    // the first attempt, no delay).
    ASSERT_EQ(responses.size(), 2u);
    for (const auto &r : responses) EXPECT_EQ(r.second, Outcome_Status_t::ENTANGLEMENT_FAILURE);

    // alice<->bob succeeds once its short delay elapses.
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    ent_manager.update_requests();
    ASSERT_EQ(commands.size(), 2u);
    std::vector<uint16_t> command_node_ids = {static_cast<uint16_t>(commands[0].first), static_cast<uint16_t>(commands[1].first)};
    EXPECT_NE(std::find(command_node_ids.begin(), command_node_ids.end(), alice), command_node_ids.end());
    EXPECT_NE(std::find(command_node_ids.begin(), command_node_ids.end(), bob), command_node_ids.end());
}

// A node pair with NO entanglement_links entry at all fails fast when
// attempted, per EntanglementManager's fail-fast-on-missing-link-model
// behavior -- confirmed here via the config-loading path rather than a
// directly-constructed map, closing the loop between "config omits a link"
// and "that pair is unusable for entanglement."
TEST_F(EntanglementConfigIntegrationTest, NodePairWithNoConfiguredLinkFailsFast) {
    const std::string yaml =
        "nodes:\n"
        "  - name: alice\n"
        "    qubit_arrays:\n"
        "      - num_qubits: 1\n"
        "        noise_model:\n"
        "          type: BASIC\n"
        "          t2_value: 1000\n"
        "  - name: bob\n"
        "    qubit_arrays:\n"
        "      - num_qubits: 1\n"
        "        noise_model:\n"
        "          type: BASIC\n"
        "          t2_value: 1000\n";
    // No entanglement_links section at all.

    const auto config = load_config(write_config(yaml).string());
    ASSERT_TRUE(config.has_value());
    ASSERT_TRUE(config->entanglement_links.empty());

    auto sys = resolve_system(*config);
    ASSERT_TRUE(sys.attempt_models.empty());
    const uint16_t alice = sys.name_to_node_id.at("alice");
    const uint16_t bob = sys.name_to_node_id.at("bob");

    std::vector<std::pair<int, Outcome_Status_t>> responses;
    EntanglementManager ent_manager(
        [&](int node_id, Outcome_Status_t status) { responses.emplace_back(node_id, status); },
        [&](int, std::unique_ptr<CommandBase>) { FAIL() << "should never succeed without a configured link"; });
    ent_manager.configure_links(std::move(sys.attempt_models));

    ent_manager.add_new_request(make_entanglement_attempt(bob), alice, bob);
    ent_manager.add_new_request(make_entanglement_attempt(alice), bob, alice);

    ASSERT_EQ(responses.size(), 2u);
    EXPECT_EQ(responses[0].second, Outcome_Status_t::ENTANGLEMENT_FAILURE);
    EXPECT_EQ(responses[1].second, Outcome_Status_t::ENTANGLEMENT_FAILURE);
}

} // namespace
