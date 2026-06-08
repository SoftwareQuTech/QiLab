#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <variant>

#include "ConfigurationEngine/config_loader.hpp"

namespace {

// Builds a minimal valid single-node config around a given qubit noise model block.
// Caller provides the indented noise_model lines (6-space indent).
static std::string single_node_with_noise(const std::string& noise_model_block)
{
    return std::string("nodes:\n")
         + "  - name: fixture_node\n"
         + "    node_type: GENERIC\n"
         + "    qubit_arrays:\n"
         + "      - num_qubits: 1\n"
         + "        noise_model:\n"
         + noise_model_block;
}

class EnvVarGuard {
public:
    explicit EnvVarGuard(const char* var_name)
        : name(var_name)
    {
        const char* value = std::getenv(name.c_str());
        if (value != nullptr)
        {
            had_value = true;
            original_value = value;
        }
    }

    ~EnvVarGuard()
    {
        if (had_value)
        {
            setenv(name.c_str(), original_value.c_str(), 1);
        }
        else
        {
            unsetenv(name.c_str());
        }
    }

private:
    std::string name;
    bool had_value = false;
    std::string original_value;
};

class ConfigLoaderTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        const auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        temp_dir = std::filesystem::temp_directory_path() /
                   ("qilab_config_loader_test_" + std::to_string(now));
        std::filesystem::create_directories(temp_dir);
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove_all(temp_dir, ec);
    }

    std::filesystem::path write_config(const std::string& file_name, const std::string& content)
    {
        const std::filesystem::path path = temp_dir / file_name;
        std::ofstream out(path);
        EXPECT_TRUE(out.is_open());
        out << content;
        out.close();
        return path;
    }

    std::filesystem::path temp_dir;
};

TEST_F(ConfigLoaderTest, LoadsModernQubitArraysAndEntanglementModel)
{
    const std::string yaml =
        "entanglement_noise_model:\n"
        "  type: DEPOLARIZING\n"
        "  error_rate: 0.15\n"
        "  gate_time_ns: 50\n"
        "nodes:\n"
        "  - name: nodeA\n"
        "    node_type: PROCESSOR\n"
        "    qubit_arrays:\n"
        "      - num_qubits: 1\n"
        "        noise_model:\n"
        "          type: BASIC\n"
        "          t2_value: 1200\n"
        "      - num_qubits: 2\n"
        "        noise_model:\n"
        "          type: PHASE_FLIP\n"
        "          t2_value: 100\n";

    const auto path = write_config("modern.yaml", yaml);

    const auto config = load_config(path.string());
    ASSERT_TRUE(config.has_value());
    ASSERT_TRUE(std::holds_alternative<DepolarizingEntanglementNoiseConfig>(config->entanglement_noise_model));
    ASSERT_EQ(config->nodes.size(), 1u);

    const auto& node = config->nodes.front();
    EXPECT_EQ(node.name, "nodeA");
    EXPECT_EQ(node.node_type, QuantumNodeType::PROCESSOR);
    EXPECT_EQ(node.num_qubits, 3);
    ASSERT_EQ(node.qubit_arrays.size(), 2u);
    EXPECT_EQ(node.qubit_arrays[0].num_qubits, 1);
    EXPECT_EQ(node.qubit_arrays[1].num_qubits, 2);

    // EXPECT_TRUE(std::holds_alternative<QubitNoiseBasicConfig>(node.qubit_noise_model.config_data));
    EXPECT_TRUE(std::holds_alternative<PhaseFlipNoiseConfig>(node.qubit_arrays[1].noise_model.config_data));
}

TEST_F(ConfigLoaderTest, ParsesQuantumOperationDelays)
{
    const std::string yaml =
        "nodes:\n"
        "  - name: node_with_delays\n"
        "    quantum_operations_delay_ns:\n"
        "      single_gate_rotation: 11\n"
        "      two_qubit_gate_rotation: 22\n"
        "      measurement: 33\n"
        "      entanglement: 44\n"
        "      qubit_move: 55\n"
        "      qubit_swap: 66\n"
        "      qubit_init: 77\n"
        "    node_type: GENERIC\n"
        "    qubit_arrays:\n"
        "      - num_qubits: 1\n"
        "        noise_model:\n"
        "          type: BASIC\n"
        "          t2_value: 1000\n";

    const auto path = write_config("node_with_delays.yaml", yaml);
    const auto config = load_config(path.string());

    ASSERT_TRUE(config.has_value());
    ASSERT_EQ(config->nodes.size(), 1u);

    const auto& delays = config->nodes.front().qubit_operations_delays;
    EXPECT_EQ(delays.single_gate_delay, 11);
    EXPECT_EQ(delays.two_gate_delay, 22);
    EXPECT_EQ(delays.measurement_delay, 33);
    EXPECT_EQ(delays.entanglement_delay, 44);
    EXPECT_EQ(delays.qubit_move_delay, 55);
    EXPECT_EQ(delays.qubit_swap_delay, 66);
    EXPECT_EQ(delays.qubit_init_delay, 77);
}

TEST_F(ConfigLoaderTest, RejectsUnknownTopLevelKey)
{
    const std::string yaml =
        "unknown_section:\n"
        "  value: 1\n"
        "nodes:\n"
        "  - name: top_level_case\n"
        "    node_type: GENERIC\n"
        "    qubit_arrays:\n"
        "      - num_qubits: 1\n"
        "        noise_model:\n"
        "          type: BASIC\n"
        "          t2_value: 1000\n";

    const auto path = write_config("unknown_top.yaml", yaml);
    const auto config = load_config(path.string());
    EXPECT_FALSE(config.has_value());
}

TEST_F(ConfigLoaderTest, RejectsUnknownNodeKey)
{
    const std::string yaml =
        "nodes:\n"
        "  - unexpected: 5\n"
        "    qubit_arrays:\n"
        "      - num_qubits: 1\n"
        "        noise_model:\n"
        "          type: BASIC\n"
        "          t2_value: 1000\n";

    const auto path = write_config("unknown_node_key.yaml", yaml);
    const auto config = load_config(path.string());
    EXPECT_FALSE(config.has_value());
}

TEST_F(ConfigLoaderTest, RejectsUnsupportedQubitNoiseModelType)
{
    const std::string yaml =
        "nodes:\n"
        "  - name: bad_model\n"
        "    node_type: GENERIC\n"
        "    qubit_arrays:\n"
        "      - num_qubits: 2\n"
        "        noise_model:\n"
        "          type: NOT_A_MODEL\n";

    const auto path = write_config("unsupported_model.yaml", yaml);
    const auto config = load_config(path.string());
    EXPECT_FALSE(config.has_value());
}

TEST_F(ConfigLoaderTest, RejectsUnknownFieldInsideModelBlock)
{
    const std::string yaml =
        "nodes:\n"
        "  - name: bad_field\n"
        "    node_type: GENERIC\n"
        "    qubit_arrays:\n"
        "      - num_qubits: 2\n"
        "        noise_model:\n"
        "          type: BASIC\n"
        "          t2_value: 1000\n"
        "          extra_field: 1\n";

    const auto path = write_config("unknown_model_field.yaml", yaml);
    const auto config = load_config(path.string());
    EXPECT_FALSE(config.has_value());
}

TEST_F(ConfigLoaderTest, RejectsMissingNodesSection)
{
    const std::string yaml =
        "entanglement_noise_model:\n"
        "  type: BASIC\n"
        "  t2_value: 1000\n";

    const auto path = write_config("missing_nodes.yaml", yaml);
    const auto config = load_config(path.string());
    EXPECT_FALSE(config.has_value());
}

TEST_F(ConfigLoaderTest, ResolvesDefaultPathByEnvironmentPrecedence)
{
    EnvVarGuard path_guard("QILAB_CONFIG_PATH");
    EnvVarGuard dir_guard("QILAB_CONFIG_DIR");

    const std::string explicit_path = (temp_dir / "explicit.yaml").string();
    setenv("QILAB_CONFIG_PATH", explicit_path.c_str(), 1);
    setenv("QILAB_CONFIG_DIR", "/tmp/ignored_dir", 1);

    EXPECT_EQ(get_default_config_path(), explicit_path);

    unsetenv("QILAB_CONFIG_PATH");
    setenv("QILAB_CONFIG_DIR", temp_dir.string().c_str(), 1);
    EXPECT_EQ(get_default_config_path(), (temp_dir / "nodes.yaml").string());
}

TEST_F(ConfigLoaderTest, CreatesDefaultConfigFileInConfiguredDirectory)
{
    EnvVarGuard path_guard("QILAB_CONFIG_PATH");
    EnvVarGuard dir_guard("QILAB_CONFIG_DIR");

    unsetenv("QILAB_CONFIG_PATH");
    setenv("QILAB_CONFIG_DIR", temp_dir.string().c_str(), 1);

    ASSERT_TRUE(create_default_config());

    const std::filesystem::path expected = temp_dir / "nodes.yaml";
    ASSERT_TRUE(std::filesystem::exists(expected));

    std::ifstream in(expected);
    ASSERT_TRUE(in.is_open());
    const std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    EXPECT_NE(content.find("nodes:"), std::string::npos);
    EXPECT_NE(content.find("entanglement_noise_model:"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Multiple nodes
// ---------------------------------------------------------------------------

TEST_F(ConfigLoaderTest, LoadsMultipleNodes)
{
    const std::string yaml =
        "nodes:\n"
        "  - name: alice\n"
        "    node_type: NV_CENTER\n"
        "    qubit_arrays:\n"
        "      - num_qubits: 3\n"
        "        noise_model:\n"
        "          type: BASIC\n"
        "          t2_value: 500\n"
        "  - name: bob\n"
        "    node_type: ION_TRAP\n"
        "    qubit_arrays:\n"
        "      - num_qubits: 5\n"
        "        noise_model:\n"
        "          type: BASIC\n"
        "          t2_value: 800\n"
        "  - name: relay\n"
        "    node_type: GENERIC\n"
        "    qubit_arrays:\n"
        "      - num_qubits: 2\n"
        "        noise_model:\n"
        "          type: BASIC\n"
        "          t2_value: 200\n";

    const auto path = write_config("multi_node.yaml", yaml);
    const auto config = load_config(path.string());
    ASSERT_TRUE(config.has_value());
    ASSERT_EQ(config->nodes.size(), 3u);

    EXPECT_EQ(config->nodes[0].name, "alice");
    EXPECT_EQ(config->nodes[0].node_type, QuantumNodeType::NV_CENTER);
    EXPECT_EQ(config->nodes[0].num_qubits, 3);

    EXPECT_EQ(config->nodes[1].name, "bob");
    EXPECT_EQ(config->nodes[1].node_type, QuantumNodeType::ION_TRAP);
    EXPECT_EQ(config->nodes[1].num_qubits, 5);

    EXPECT_EQ(config->nodes[2].node_type, QuantumNodeType::GENERIC);
    EXPECT_EQ(config->nodes[2].num_qubits, 2);
}

TEST_F(ConfigLoaderTest, NumQubitsIsSumOfAllArrays)
{
    const std::string yaml =
        "nodes:\n"
        "  - name: alice\n"
        "    node_type: NV_CENTER\n"
        "    qubit_arrays:\n"
        "      - num_qubits: 2\n"
        "        noise_model:\n"
        "          type: BASIC\n"
        "          t2_value: 1000\n"
        "      - num_qubits: 3\n"
        "        noise_model:\n"
        "          type: BASIC\n"
        "          t2_value: 1000\n"
        "      - num_qubits: 5\n"
        "        noise_model:\n"
        "          type: BASIC\n"
        "          t2_value: 1000\n";

    const auto path = write_config("sum_qubits.yaml", yaml);
    const auto config = load_config(path.string());
    ASSERT_TRUE(config.has_value());
    EXPECT_EQ(config->nodes.front().num_qubits, 10);
    EXPECT_EQ(config->nodes.front().qubit_arrays.size(), 3u);
}

TEST_F(ConfigLoaderTest, NodeLevelNoiseModelFollowsFirstArray)
{
    const std::string yaml =
        "nodes:\n"
        "  - name: first_model_case\n"
        "    node_type: GENERIC\n"
        "    qubit_arrays:\n"
        "      - num_qubits: 1\n"
        "        noise_model:\n"
        "          type: PHASE_FLIP\n"
        "          t2_value: 100\n"
        "      - num_qubits: 1\n"
        "        noise_model:\n"
        "          type: BASIC\n"
        "          t2_value: 1000\n";

    const auto path = write_config("first_array_model.yaml", yaml);
    const auto config = load_config(path.string());
    ASSERT_TRUE(config.has_value());
    // EXPECT_TRUE(std::holds_alternative<PhaseFlipNoiseConfig>(
    //    config->nodes.front().qubit_noise_model.config_data));
}

// ---------------------------------------------------------------------------
// Node types
// ---------------------------------------------------------------------------

TEST_F(ConfigLoaderTest, AllNodeTypesAreAccepted)
{
    const std::vector<std::pair<std::string, QuantumNodeType>> cases = {
        {"GENERIC",   QuantumNodeType::GENERIC},
        {"NV_CENTER", QuantumNodeType::NV_CENTER},
        {"ION_TRAP",  QuantumNodeType::ION_TRAP},
        {"PROCESSOR", QuantumNodeType::PROCESSOR},
    };

    for (const auto& [type_str, expected_type] : cases)
    {
        const std::string yaml =
            "nodes:\n"
            "  - name: typed_node\n"
            "    node_type: " + type_str + "\n"
            "    qubit_arrays:\n"
            "      - num_qubits: 1\n"
            "        noise_model:\n"
            "          type: BASIC\n"
            "          t2_value: 1000\n";

        const auto path = write_config("node_type_" + type_str + ".yaml", yaml);
        const auto config = load_config(path.string());
        ASSERT_TRUE(config.has_value()) << "Failed for node_type: " << type_str;
        EXPECT_EQ(config->nodes.front().node_type, expected_type);
    }
}

TEST_F(ConfigLoaderTest, RejectsInvalidNodeType)
{
    const std::string yaml =
        "nodes:\n"
        "  - node_type: QUANTUM_COMPUTER\n"
        "    qubit_arrays:\n"
        "      - num_qubits: 1\n"
        "        noise_model:\n"
        "          type: BASIC\n"
        "          t2_value: 1000\n";

    const auto path = write_config("invalid_node_type.yaml", yaml);
    EXPECT_FALSE(load_config(path.string()).has_value());
}

// ---------------------------------------------------------------------------
// Field order independence
// ---------------------------------------------------------------------------

TEST_F(ConfigLoaderTest, NodeFieldOrderDoesNotMatter)
{
    // qubit_arrays first, then name and node_type
    const std::string yaml =
        "nodes:\n"
        "  - qubit_arrays:\n"
        "      - num_qubits: 4\n"
        "        noise_model:\n"
        "          type: BASIC\n"
        "          t2_value: 1000\n"
        "    node_type: PROCESSOR\n"
        "    name: reordered\n";

    const auto path = write_config("node_field_order.yaml", yaml);
    const auto config = load_config(path.string());
    ASSERT_TRUE(config.has_value());
    EXPECT_EQ(config->nodes.front().name, "reordered");
    EXPECT_EQ(config->nodes.front().node_type, QuantumNodeType::PROCESSOR);
    EXPECT_EQ(config->nodes.front().num_qubits, 4);
}

TEST_F(ConfigLoaderTest, QubitArrayFieldOrderDoesNotMatter)
{
    // noise_model before num_qubits
    const std::string yaml =
        "nodes:\n"
        "  - name: array_order\n"
        "    node_type: GENERIC\n"
        "    qubit_arrays:\n"
        "      - noise_model:\n"
        "          type: BASIC\n"
        "          t2_value: 750\n"
        "        num_qubits: 3\n";

    const auto path = write_config("array_field_order.yaml", yaml);
    const auto config = load_config(path.string());
    ASSERT_TRUE(config.has_value());
    EXPECT_EQ(config->nodes.front().qubit_arrays.front().num_qubits, 3);
    const auto& model = config->nodes.front().qubit_arrays.front().noise_model.config_data;
    ASSERT_TRUE(std::holds_alternative<QubitNoiseBasicConfig>(model));
    EXPECT_FLOAT_EQ(std::get<QubitNoiseBasicConfig>(model).t2_value, 750.0f);
}

TEST_F(ConfigLoaderTest, TopLevelFieldOrderDoesNotMatter)
{
    // nodes before entanglement_noise_model
    const std::string yaml =
        "nodes:\n"
        "  - name: top_order\n"
        "    node_type: GENERIC\n"
        "    qubit_arrays:\n"
        "      - num_qubits: 1\n"
        "        noise_model:\n"
        "          type: BASIC\n"
        "          t2_value: 1000\n"
        "entanglement_noise_model:\n"
        "  type: BASIC\n"
        "  t2_value: 500\n";

    const auto path = write_config("top_level_order.yaml", yaml);
    const auto config = load_config(path.string());
    ASSERT_TRUE(config.has_value());
    ASSERT_TRUE(std::holds_alternative<EntanglementNoiseBasicConfig>(config->entanglement_noise_model));
    EXPECT_FLOAT_EQ(std::get<EntanglementNoiseBasicConfig>(config->entanglement_noise_model).t2_value, 500.0f);
}

TEST_F(ConfigLoaderTest, CommentsAndBlankLinesAreIgnored)
{
    const std::string yaml =
        "# Top-level comment\n"
        "\n"
        "nodes:\n"
        "  # Node comment\n"
        "  - name: test\n"
        "    node_type: GENERIC\n"
        "\n"
        "    qubit_arrays:\n"
        "      # Array comment\n"
        "      - num_qubits: 2\n"
        "        noise_model:\n"
        "          type: BASIC\n"
        "          t2_value: 1000\n";

    const auto path = write_config("comments.yaml", yaml);
    const auto config = load_config(path.string());
    ASSERT_TRUE(config.has_value());
    EXPECT_EQ(config->nodes.front().name, "test");
    EXPECT_EQ(config->nodes.front().num_qubits, 2);
}

// ---------------------------------------------------------------------------
// Qubit noise model: BASIC
// ---------------------------------------------------------------------------

TEST_F(ConfigLoaderTest, QubitNoiseBasicLoadsCorrectValues)
{
    const std::string yaml = single_node_with_noise(
        "          type: BASIC\n"
        "          t2_value: 2500\n");

    const auto path = write_config("qubit_basic.yaml", yaml);
    const auto config = load_config(path.string());
    ASSERT_TRUE(config.has_value());
    const auto& model = config->nodes.front().qubit_arrays.front().noise_model.config_data;
    ASSERT_TRUE(std::holds_alternative<QubitNoiseBasicConfig>(model));
    EXPECT_FLOAT_EQ(std::get<QubitNoiseBasicConfig>(model).t2_value, 2500.0f);
}

TEST_F(ConfigLoaderTest, QubitNoiseBasicRejectsMissingT2Value)
{
    const std::string yaml = single_node_with_noise("          type: BASIC\n");
    const auto path = write_config("basic_no_t2.yaml", yaml);
    EXPECT_FALSE(load_config(path.string()).has_value());
}

TEST_F(ConfigLoaderTest, QubitNoiseBasicRejectsInvalidT2Value)
{
    const std::string yaml = single_node_with_noise(
        "          type: BASIC\n"
        "          t2_value: not_a_number\n");
    const auto path = write_config("basic_bad_t2.yaml", yaml);
    EXPECT_FALSE(load_config(path.string()).has_value());
}

TEST_F(ConfigLoaderTest, QubitNoiseBasicRejectsNegativeT2Value)
{
    const std::string yaml = single_node_with_noise(
        "          type: BASIC\n"
        "          t2_value: -100\n");
    const auto path = write_config("basic_neg_t2.yaml", yaml);
    EXPECT_FALSE(load_config(path.string()).has_value());
}

// ---------------------------------------------------------------------------
// Qubit noise model: ADVANCED
// ---------------------------------------------------------------------------

TEST_F(ConfigLoaderTest, QubitNoiseAdvancedLoadsCorrectValues)
{
    const std::string yaml = single_node_with_noise(
        "          type: ADVANCED\n"
        "          t1_value: 6000\n"
        "          t2_value: 3000\n"
        "          gate_error_rate: 0.002\n");

    const auto path = write_config("qubit_advanced.yaml", yaml);
    const auto config = load_config(path.string());
    ASSERT_TRUE(config.has_value());
    const auto& model = config->nodes.front().qubit_arrays.front().noise_model.config_data;
    ASSERT_TRUE(std::holds_alternative<QubitNoiseAdvancedConfig>(model));
    const auto& adv = std::get<QubitNoiseAdvancedConfig>(model);
    EXPECT_FLOAT_EQ(adv.t1_value, 6000.0f);
    EXPECT_FLOAT_EQ(adv.t2_value, 3000.0f);
    EXPECT_FLOAT_EQ(adv.gate_error_rate, 0.002f);
}

TEST_F(ConfigLoaderTest, QubitNoiseAdvancedRejectsMissingT1Value)
{
    const std::string yaml = single_node_with_noise(
        "          type: ADVANCED\n"
        "          t2_value: 3000\n"
        "          gate_error_rate: 0.001\n");
    const auto path = write_config("advanced_no_t1.yaml", yaml);
    EXPECT_FALSE(load_config(path.string()).has_value());
}

TEST_F(ConfigLoaderTest, QubitNoiseAdvancedRejectsMissingT2Value)
{
    const std::string yaml = single_node_with_noise(
        "          type: ADVANCED\n"
        "          t1_value: 6000\n"
        "          gate_error_rate: 0.001\n");
    const auto path = write_config("advanced_no_t2.yaml", yaml);
    EXPECT_FALSE(load_config(path.string()).has_value());
}

TEST_F(ConfigLoaderTest, QubitNoiseAdvancedRejectsMissingGateErrorRate)
{
    const std::string yaml = single_node_with_noise(
        "          type: ADVANCED\n"
        "          t1_value: 6000\n"
        "          t2_value: 3000\n");
    const auto path = write_config("advanced_no_gate_error.yaml", yaml);
    EXPECT_FALSE(load_config(path.string()).has_value());
}

// ---------------------------------------------------------------------------
// Qubit noise model: DEPOLARIZING
// ---------------------------------------------------------------------------

TEST_F(ConfigLoaderTest, QubitNoiseDepolarizingLoadsCorrectValues)
{
    const std::string yaml = single_node_with_noise(
        "          type: DEPOLARIZING\n"
        "          error_rate: 0.03\n"
        "          gate_time_ns: 75\n");

    const auto path = write_config("qubit_depol.yaml", yaml);
    const auto config = load_config(path.string());
    ASSERT_TRUE(config.has_value());
    const auto& model = config->nodes.front().qubit_arrays.front().noise_model.config_data;
    ASSERT_TRUE(std::holds_alternative<DepolarizingQubitNoiseConfig>(model));
    const auto& depol = std::get<DepolarizingQubitNoiseConfig>(model);
    EXPECT_FLOAT_EQ(depol.error_rate, 0.03f);
    EXPECT_FLOAT_EQ(depol.gate_time_ns, 75.0f);
}

TEST_F(ConfigLoaderTest, QubitNoiseDepolarizingRejectsMissingErrorRate)
{
    const std::string yaml = single_node_with_noise(
        "          type: DEPOLARIZING\n"
        "          gate_time_ns: 75\n");
    const auto path = write_config("depol_no_error_rate.yaml", yaml);
    EXPECT_FALSE(load_config(path.string()).has_value());
}

TEST_F(ConfigLoaderTest, QubitNoiseDepolarizingRejectsMissingGateTimeNs)
{
    const std::string yaml = single_node_with_noise(
        "          type: DEPOLARIZING\n"
        "          error_rate: 0.03\n");
    const auto path = write_config("depol_no_gate_time.yaml", yaml);
    EXPECT_FALSE(load_config(path.string()).has_value());
}

// ---------------------------------------------------------------------------
// Qubit noise model: AMPLITUDE_DAMPING
// ---------------------------------------------------------------------------

TEST_F(ConfigLoaderTest, QubitNoiseAmplitudeDampingLoadsCorrectValues)
{
    const std::string yaml = single_node_with_noise(
        "          type: AMPLITUDE_DAMPING\n"
        "          t1_ns: 4000\n"
        "          t2_ns: 2000\n");

    const auto path = write_config("qubit_amp_damp.yaml", yaml);
    const auto config = load_config(path.string());
    ASSERT_TRUE(config.has_value());
    const auto& model = config->nodes.front().qubit_arrays.front().noise_model.config_data;
    ASSERT_TRUE(std::holds_alternative<AmplitudeDampingNoiseConfig>(model));
    const auto& amp = std::get<AmplitudeDampingNoiseConfig>(model);
    EXPECT_FLOAT_EQ(amp.t1_ns, 4000.0f);
    EXPECT_FLOAT_EQ(amp.t2_ns, 2000.0f);
}

TEST_F(ConfigLoaderTest, QubitNoiseAmplitudeDampingRejectsMissingT1Ns)
{
    const std::string yaml = single_node_with_noise(
        "          type: AMPLITUDE_DAMPING\n"
        "          t2_ns: 2000\n");
    const auto path = write_config("amp_damp_no_t1.yaml", yaml);
    EXPECT_FALSE(load_config(path.string()).has_value());
}

TEST_F(ConfigLoaderTest, QubitNoiseAmplitudeDampingRejectsMissingT2Ns)
{
    const std::string yaml = single_node_with_noise(
        "          type: AMPLITUDE_DAMPING\n"
        "          t1_ns: 4000\n");
    const auto path = write_config("amp_damp_no_t2.yaml", yaml);
    EXPECT_FALSE(load_config(path.string()).has_value());
}

// ---------------------------------------------------------------------------
// Qubit noise model: PHASE_FLIP
// ---------------------------------------------------------------------------

TEST_F(ConfigLoaderTest, QubitNoisePhaseFlipLoadsCorrectValues)
{
    const std::string yaml = single_node_with_noise(
        "          type: PHASE_FLIP\n"
        "          t2_value: 100\n");

    const auto path = write_config("qubit_phase_flip.yaml", yaml);
    const auto config = load_config(path.string());
    ASSERT_TRUE(config.has_value());
    const auto& model = config->nodes.front().qubit_arrays.front().noise_model.config_data;
    ASSERT_TRUE(std::holds_alternative<PhaseFlipNoiseConfig>(model));
    EXPECT_FLOAT_EQ(std::get<PhaseFlipNoiseConfig>(model).t2_value, 100);
}

TEST_F(ConfigLoaderTest, QubitNoisePhaseFlipRejectsMissingFlipProbability)
{
    const std::string yaml = single_node_with_noise("          type: PHASE_FLIP\n");
    const auto path = write_config("phase_flip_no_prob.yaml", yaml);
    EXPECT_FALSE(load_config(path.string()).has_value());
}

// ---------------------------------------------------------------------------
// Model block structure
// ---------------------------------------------------------------------------

TEST_F(ConfigLoaderTest, RejectsMissingTypeInQubitNoiseModel)
{
    const std::string yaml = single_node_with_noise("          t2_value: 1000\n");
    const auto path = write_config("no_type.yaml", yaml);
    EXPECT_FALSE(load_config(path.string()).has_value());
}

TEST_F(ConfigLoaderTest, RejectsDuplicateKeyInModelBlock)
{
    const std::string yaml = single_node_with_noise(
        "          type: BASIC\n"
        "          t2_value: 1000\n"
        "          t2_value: 2000\n");
    const auto path = write_config("duplicate_key.yaml", yaml);
    EXPECT_FALSE(load_config(path.string()).has_value());
}

TEST_F(ConfigLoaderTest, AcceptsZeroFloatForNoiseParams)
{
    const std::string yaml = single_node_with_noise(
        "          type: BASIC\n"
        "          t2_value: 0\n");
    const auto path = write_config("zero_t2.yaml", yaml);
    const auto config = load_config(path.string());
    ASSERT_TRUE(config.has_value());
    const auto& model = config->nodes.front().qubit_arrays.front().noise_model.config_data;
    EXPECT_FLOAT_EQ(std::get<QubitNoiseBasicConfig>(model).t2_value, 0.0f);
}

// ---------------------------------------------------------------------------
// Entanglement noise model
// ---------------------------------------------------------------------------

TEST_F(ConfigLoaderTest, DefaultEntanglementModelIsBasicWhenAbsent)
{
    const std::string yaml = single_node_with_noise(
        "          type: BASIC\n"
        "          t2_value: 1000\n");

    const auto path = write_config("no_entanglement_model.yaml", yaml);
    const auto config = load_config(path.string());
    ASSERT_TRUE(config.has_value());
    ASSERT_TRUE(std::holds_alternative<EntanglementNoiseBasicConfig>(config->entanglement_noise_model));
    EXPECT_FLOAT_EQ(std::get<EntanglementNoiseBasicConfig>(config->entanglement_noise_model).t2_value, 1000.0f);
}

TEST_F(ConfigLoaderTest, EntanglementNoiseBasicLoadsCorrectValues)
{
    const std::string yaml =
        "entanglement_noise_model:\n"
        "  type: BASIC\n"
        "  t2_value: 3500\n"
        + std::string(single_node_with_noise(
            "          type: BASIC\n"
            "          t2_value: 1000\n"));

    const auto path = write_config("entanglement_basic.yaml", yaml);
    const auto config = load_config(path.string());
    ASSERT_TRUE(config.has_value());
    ASSERT_TRUE(std::holds_alternative<EntanglementNoiseBasicConfig>(config->entanglement_noise_model));
    EXPECT_FLOAT_EQ(std::get<EntanglementNoiseBasicConfig>(config->entanglement_noise_model).t2_value, 3500.0f);
}

TEST_F(ConfigLoaderTest, EntanglementNoiseDepolarizingLoadsCorrectValues)
{
    const std::string yaml =
        "entanglement_noise_model:\n"
        "  type: DEPOLARIZING\n"
        "  error_rate: 0.08\n"
        "  gate_time_ns: 300\n"
        + std::string(single_node_with_noise(
            "          type: BASIC\n"
            "          t2_value: 1000\n"));

    const auto path = write_config("entanglement_depol.yaml", yaml);
    const auto config = load_config(path.string());
    ASSERT_TRUE(config.has_value());
    ASSERT_TRUE(std::holds_alternative<DepolarizingEntanglementNoiseConfig>(config->entanglement_noise_model));
    const auto& depol = std::get<DepolarizingEntanglementNoiseConfig>(config->entanglement_noise_model);
    EXPECT_FLOAT_EQ(depol.error_rate, 0.08f);
    EXPECT_FLOAT_EQ(depol.gate_time_ns, 300.0f);
}

TEST_F(ConfigLoaderTest, RejectsUnsupportedEntanglementNoiseModelType)
{
    const std::string yaml =
        "entanglement_noise_model:\n"
        "  type: UNKNOWN_TYPE\n"
        "  t2_value: 1000\n"
        + std::string(single_node_with_noise(
            "          type: BASIC\n"
            "          t2_value: 1000\n"));

    const auto path = write_config("bad_entanglement_type.yaml", yaml);
    EXPECT_FALSE(load_config(path.string()).has_value());
}

TEST_F(ConfigLoaderTest, RejectsMissingTypeInEntanglementModel)
{
    const std::string yaml =
        "entanglement_noise_model:\n"
        "  t2_value: 1000\n"
        + std::string(single_node_with_noise(
            "          type: BASIC\n"
            "          t2_value: 1000\n"));

    const auto path = write_config("entanglement_no_type.yaml", yaml);
    EXPECT_FALSE(load_config(path.string()).has_value());
}

TEST_F(ConfigLoaderTest, RejectsEntanglementNoiseModelWithInlineValue)
{
    const std::string yaml =
        "entanglement_noise_model: BASIC\n"
        + std::string(single_node_with_noise(
            "          type: BASIC\n"
            "          t2_value: 1000\n"));

    const auto path = write_config("entanglement_inline.yaml", yaml);
    EXPECT_FALSE(load_config(path.string()).has_value());
}

// ---------------------------------------------------------------------------
// Qubit array structural errors
// ---------------------------------------------------------------------------

TEST_F(ConfigLoaderTest, RejectsMissingQubitArrays)
{
    const std::string yaml =
        "nodes:\n"
        "  - name: no_arrays\n"
        "    node_type: GENERIC\n";

    const auto path = write_config("missing_arrays.yaml", yaml);
    EXPECT_FALSE(load_config(path.string()).has_value());
}

TEST_F(ConfigLoaderTest, RejectsEmptyQubitArraysList)
{
    const std::string yaml =
        "nodes:\n"
        "  - name: empty_arrays\n"
        "    node_type: GENERIC\n"
        "    qubit_arrays:\n";

    const auto path = write_config("empty_arrays.yaml", yaml);
    EXPECT_FALSE(load_config(path.string()).has_value());
}

TEST_F(ConfigLoaderTest, RejectsMissingNumQubitsInArray)
{
    const std::string yaml =
        "nodes:\n"
        "  - name: no_num\n"
        "    node_type: GENERIC\n"
        "    qubit_arrays:\n"
        "      - noise_model:\n"
        "          type: BASIC\n"
        "          t2_value: 1000\n";

    const auto path = write_config("array_no_num_qubits.yaml", yaml);
    EXPECT_FALSE(load_config(path.string()).has_value());
}

TEST_F(ConfigLoaderTest, RejectsMissingNoiseModelInArray)
{
    const std::string yaml =
        "nodes:\n"
        "  - name: no_noise\n"
        "    node_type: GENERIC\n"
        "    qubit_arrays:\n"
        "      - num_qubits: 2\n";

    const auto path = write_config("array_no_noise_model.yaml", yaml);
    EXPECT_FALSE(load_config(path.string()).has_value());
}

TEST_F(ConfigLoaderTest, RejectsZeroNumQubitsInArray)
{
    const std::string yaml =
        "nodes:\n"
        "  - name: zero_qubits\n"
        "    node_type: GENERIC\n"
        "    qubit_arrays:\n"
        "      - num_qubits: 0\n"
        "        noise_model:\n"
        "          type: BASIC\n"
        "          t2_value: 1000\n";

    const auto path = write_config("zero_qubits.yaml", yaml);
    EXPECT_FALSE(load_config(path.string()).has_value());
}

TEST_F(ConfigLoaderTest, RejectsNegativeNumQubitsInArray)
{
    const std::string yaml =
        "nodes:\n"
        "  - name: neg_qubits\n"
        "    node_type: GENERIC\n"
        "    qubit_arrays:\n"
        "      - num_qubits: -4\n"
        "        noise_model:\n"
        "          type: BASIC\n"
        "          t2_value: 1000\n";

    const auto path = write_config("neg_qubits.yaml", yaml);
    EXPECT_FALSE(load_config(path.string()).has_value());
}

TEST_F(ConfigLoaderTest, RejectsNonIntegerNumQubitsInArray)
{
    const std::string yaml =
        "nodes:\n"
        "  - name: float_qubits\n"
        "    node_type: GENERIC\n"
        "    qubit_arrays:\n"
        "      - num_qubits: 2.5\n"
        "        noise_model:\n"
        "          type: BASIC\n"
        "          t2_value: 1000\n";

    const auto path = write_config("float_qubits.yaml", yaml);
    EXPECT_FALSE(load_config(path.string()).has_value());
}

TEST_F(ConfigLoaderTest, RejectsUnknownFieldInQubitArray)
{
    const std::string yaml =
        "nodes:\n"
        "  - name: unknown_array_field\n"
        "    node_type: GENERIC\n"
        "    qubit_arrays:\n"
        "      - num_qubits: 2\n"
        "        noise_model:\n"
        "          type: BASIC\n"
        "          t2_value: 1000\n"
        "        extra: bad\n";

    const auto path = write_config("unknown_array_field.yaml", yaml);
    EXPECT_FALSE(load_config(path.string()).has_value());
}

TEST_F(ConfigLoaderTest, RejectsNoiseModelWithInlineValue)
{
    const std::string yaml =
        "nodes:\n"
        "  - name: inline_noise_model\n"
        "    node_type: GENERIC\n"
        "    qubit_arrays:\n"
        "      - num_qubits: 1\n"
        "        noise_model: BASIC\n";

    const auto path = write_config("inline_noise_model.yaml", yaml);
    EXPECT_FALSE(load_config(path.string()).has_value());
}

TEST_F(ConfigLoaderTest, RejectsQubitArraysWithInlineValue)
{
    const std::string yaml =
        "nodes:\n"
        "  - name: inline_arrays\n"
        "    node_type: GENERIC\n"
        "    qubit_arrays: something\n";

    const auto path = write_config("inline_qubit_arrays.yaml", yaml);
    EXPECT_FALSE(load_config(path.string()).has_value());
}

} // namespace
