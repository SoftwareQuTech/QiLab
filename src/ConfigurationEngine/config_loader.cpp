// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "config_loader.hpp"

#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <string>
#include <unordered_set>
#include <vector>

#include "ConfigurationEngine/parse_entanglement_config.hpp"
#include "ConfigurationEngine/parse_node_config.hpp"
#include "ConfigurationEngine/parse_scalar_helpers.hpp"
#include "logger/logger_api.hpp"

namespace {

constexpr int kIndentStep = 2;
constexpr float kDefaultT2ValueNs = 1000.0f;
constexpr const char* kTypeKey = "type";
constexpr const char* kNumQubitsKey = "num_qubits";
constexpr const char* kNameKey = "name";
constexpr const char* kNodeTypeKey = "node_type";
constexpr const char* kQubitDecoherenceNoiseModelKey = "qubit_decoherence_noise_model";
constexpr const char* kGateNoiseModelKey = "gate_noise_model";
constexpr const char* kSingleGateDelayNsKey = "single_gate_delay_ns";
constexpr const char* kTwoQubitGateNoiseModelKey = "two_qubit_gate_noise_model";
constexpr const char* kQubitArraysKey = "qubit_arrays";
constexpr const char* kNodesKey = "nodes";
constexpr const char* kQuantumOperationsDelays = "quantum_operations_delay_ns";
constexpr const char* kTwoQubitGateRotationKey = "two_qubit_gate_rotation";
constexpr const char* kMeasurementKey = "measurement";
constexpr const char* kEntanglementKey = "entanglement";
constexpr const char* kQubitMoveKey = "qubit_move";
constexpr const char* kQubitSwapKey = "qubit_swap";
constexpr const char* kQubitInitKey = "qubit_init";
constexpr const char* kEntanglementLinksKey = "entanglement_links";
constexpr const char* kNodeAKey = "node_a";
constexpr const char* kNodeBKey = "node_b";
constexpr const char* kAttemptModelKey = "attempt_model";
constexpr const char* kLinkNoiseModelKey = "link_noise_model";
constexpr const char* kSystemKey = "system";
constexpr const char* kTransmissionCycleTimeKey = "transmission_cycle_time_us";
constexpr const char* kQubitNoiseEnabledKey = "qubit_noise_enabled";
constexpr const char* kEntanglementNoiseEnabledKey = "entanglement_noise_enabled";

struct ParsedLine
{
    int indent = 0;
    std::string content;
};

int count_leading_spaces(const std::string& line) noexcept
{
    int spaces = 0;
    while (spaces < static_cast<int>(line.size()) && line[static_cast<std::size_t>(spaces)] == ' ')
    {
        ++spaces;
    }

    return spaces;
}

QubitNoiseModelConfig make_default_qubit_noise_model() noexcept
{
    return QubitNoiseModelConfig{.config_data = QubitNoiseBasicConfig{.t2_value = kDefaultT2ValueNs}};
}

GateNoiseModelConfig make_default_gate_noise_model() noexcept
{
    return GateNoiseModelConfig{.config_data = GateNoiseDepolarizingConfig{.depolarizing_probability = 0.0f}};
}

bool set_parse_error(std::string& error,
                     const char* scope,
                     const std::string& detail) noexcept
{
    error = std::string("[") + scope + "] " + detail;
    return false;
}

bool set_parse_error_near(std::string& error,
                          const char* scope,
                          const std::string& detail,
                          const std::string& near_content) noexcept
{
    return set_parse_error(error, scope, detail + " near '" + near_content + "'");
}

std::vector<ParsedLine> read_config_lines(std::ifstream& stream)
{
    std::vector<ParsedLine> result;
    std::string line;
    while (std::getline(stream, line))
    {
        const int indent = count_leading_spaces(line);
        const std::string content = config_parse::trim(line.substr(static_cast<std::size_t>(indent)));
        if (content.empty() || content[0] == '#')
        {
            continue;
        }

        result.push_back(ParsedLine{.indent = indent, .content = content});
    }

    return result;
}

template <typename VariantType, typename ParserMap>
bool parse_model_block(const std::vector<ParsedLine>& lines,
                       std::size_t& index,
                       int parent_indent,
                       const ParserMap& parser_map,
                       VariantType& output,
                       std::string& error) noexcept
{
    config_parse::FieldMap model_fields;

    while (index < lines.size() && lines[index].indent > parent_indent)
    {
        if (lines[index].indent != parent_indent + kIndentStep)
        {
            return set_parse_error_near(error, "model", "Invalid indentation", lines[index].content);
        }

        const auto key_value = config_parse::split_key_value(lines[index].content);
        if (!key_value.has_value())
        {
            return set_parse_error_near(error, "model", "Invalid key/value", lines[index].content);
        }

        if (model_fields.find(key_value->first) != model_fields.end())
        {
            return set_parse_error(error, "model", "Duplicate key '" + key_value->first + "'");
        }

        model_fields.insert(*key_value);
        ++index;
    }

    std::string model_type_name;
    if (!config_parse::require_key(model_fields, kTypeKey, model_type_name, error))
    {
        return false;
    }

    const auto parser_it = parser_map.find(model_type_name);
    if (parser_it == parser_map.end())
    {
        return set_parse_error(error, "model", "Unsupported type '" + model_type_name + "'");
    }

    return parser_it->second(model_fields, output, error);
}

bool parse_inline_num_qubits(const std::string& inline_content,
                             int& num_qubits,
                             bool& has_num_qubits,
                             std::string& error,
                             const char* context_name) noexcept
{
    if (inline_content.empty())
    {
        return true;
    }

    const auto inline_kv = config_parse::split_key_value(inline_content);
    if (!inline_kv.has_value() || inline_kv->first != kNumQubitsKey)
    {
        return set_parse_error(error,
                               context_name,
                               "Unsupported inline item '" + inline_content + "' (expected 'num_qubits')");
    }

    if (!config_parse::parse_positive_int(inline_kv->second, num_qubits))
    {
        return set_parse_error(error,
                               context_name,
                               "Invalid num_qubits value '" + inline_kv->second + "'");
    }

    has_num_qubits = true;
    return true;
}

// Which of the qubit array's required keys have been seen so far. Grouped into
// a struct because the two parse entry points (inline item and nested field)
// both have to thread every flag through.
struct QubitArraySeenKeys
{
    bool num_qubits = false;
    bool decoherence_noise_model = false;
    bool gate_noise_model = false;
    bool single_gate_delay_ns = false;
};

bool parse_qubit_array_field(const std::vector<ParsedLine>& lines,
                             std::size_t& index,
                             int item_indent,
                             QubitArrayConfig& array_config,
                             QubitArraySeenKeys& seen,
                             std::string& error) noexcept
{
    const auto key_value = config_parse::split_key_value(lines[index].content);
    if (!key_value.has_value())
    {
        return set_parse_error_near(error, "qubit_array", "Invalid key/value", lines[index].content);
    }

    if (key_value->first == kNumQubitsKey)
    {
        if (!config_parse::parse_positive_int(key_value->second, array_config.num_qubits))
        {
            return set_parse_error(error, "qubit_array", "Invalid num_qubits value '" + key_value->second + "'");
        }

        seen.num_qubits = true;
        ++index;
        return true;
    }

    if (key_value->first == kSingleGateDelayNsKey)
    {
        if (!config_parse::parse_non_negative_u64(key_value->second, array_config.single_gate_delay_ns))
        {
            return set_parse_error(error,
                                   "qubit_array",
                                   "Invalid single_gate_delay_ns value '" + key_value->second + "'");
        }

        seen.single_gate_delay_ns = true;
        ++index;
        return true;
    }

    if (key_value->first == kQubitDecoherenceNoiseModelKey)
    {
        if (!key_value->second.empty())
        {
            return set_parse_error(error, "qubit_array", "qubit_decoherence_noise_model must be a nested map");
        }

        ++index;
        if (!parse_model_block(lines,
                               index,
                               item_indent + kIndentStep,
                               config_parse::qubit_model_parsers(),
                               array_config.qubit_decoherence_noise_model.config_data,
                               error))
        {
            return false;
        }

        seen.decoherence_noise_model = true;
        return true;
    }

    if (key_value->first == kGateNoiseModelKey)
    {
        if (!key_value->second.empty())
        {
            return set_parse_error(error, "qubit_array", "gate_noise_model must be a nested map");
        }

        ++index;
        if (!parse_model_block(lines,
                               index,
                               item_indent + kIndentStep,
                               config_parse::single_qubit_gate_noise_model_parsers(),
                               array_config.gate_noise_model.config_data,
                               error))
        {
            return false;
        }

        seen.gate_noise_model = true;
        return true;
    }

    return set_parse_error(error, "qubit_array", "Unknown key '" + key_value->first + "'");
}

bool parse_single_qubit_array_item(const std::vector<ParsedLine>& lines,
                                   std::size_t& index,
                                   int parent_indent,
                                   QubitArrayConfig& array_config,
                                   std::string& error) noexcept
{
    if (lines[index].indent != parent_indent + kIndentStep)
    {
        return set_parse_error_near(error, "qubit_array", "Invalid indentation", lines[index].content);
    }

    if (lines[index].content.empty() || lines[index].content[0] != '-')
    {
        return set_parse_error(error, "qubit_array", "Expected list entry under qubit_arrays");
    }

    const int item_indent = lines[index].indent;
    const std::string inline_content = config_parse::trim(lines[index].content.substr(1));
    ++index;

    QubitArraySeenKeys seen;

    if (!inline_content.empty())
    {
        const auto inline_kv = config_parse::split_key_value(inline_content);
        if (!inline_kv.has_value())
        {
            return set_parse_error(error, "qubit_array", "Invalid inline key/value '" + inline_content + "'");
        }

        if (inline_kv->first == kNumQubitsKey)
        {
            if (!config_parse::parse_positive_int(inline_kv->second, array_config.num_qubits))
            {
                return set_parse_error(error,
                                       "qubit_array",
                                       "Invalid num_qubits value '" + inline_kv->second + "'");
            }

            seen.num_qubits = true;
        }
        else if (inline_kv->first == kSingleGateDelayNsKey)
        {
            if (!config_parse::parse_non_negative_u64(inline_kv->second, array_config.single_gate_delay_ns))
            {
                return set_parse_error(error,
                                       "qubit_array",
                                       "Invalid single_gate_delay_ns value '" + inline_kv->second + "'");
            }

            seen.single_gate_delay_ns = true;
        }
        else if (inline_kv->first == kQubitDecoherenceNoiseModelKey)
        {
            if (!inline_kv->second.empty())
            {
                return set_parse_error(error, "qubit_array", "qubit_decoherence_noise_model must be a nested map");
            }

            if (!parse_model_block(lines,
                                   index,
                                   item_indent + kIndentStep,
                                   config_parse::qubit_model_parsers(),
                                   array_config.qubit_decoherence_noise_model.config_data,
                                   error))
            {
                return false;
            }

            seen.decoherence_noise_model = true;
        }
        else if (inline_kv->first == kGateNoiseModelKey)
        {
            if (!inline_kv->second.empty())
            {
                return set_parse_error(error, "qubit_array", "gate_noise_model must be a nested map");
            }

            if (!parse_model_block(lines,
                                   index,
                                   item_indent + kIndentStep,
                                   config_parse::single_qubit_gate_noise_model_parsers(),
                                   array_config.gate_noise_model.config_data,
                                   error))
            {
                return false;
            }

            seen.gate_noise_model = true;
        }
        else
        {
            return set_parse_error(error,
                                   "qubit_array",
                                   "Unsupported inline item '" + inline_content + "'");
        }
    }

    while (index < lines.size() && lines[index].indent > item_indent)
    {
        if (lines[index].indent != item_indent + kIndentStep)
        {
            return set_parse_error_near(error, "qubit_array", "Invalid indentation", lines[index].content);
        }

        if (!parse_qubit_array_field(lines, index, item_indent, array_config, seen, error))
        {
            return false;
        }
    }

    if (!seen.num_qubits)
    {
        return set_parse_error(error, "qubit_array", "Missing required key 'num_qubits'");
    }

    if (!seen.decoherence_noise_model)
    {
        return set_parse_error(error, "qubit_array", "Missing required key 'qubit_decoherence_noise_model'");
    }

    if (!seen.gate_noise_model)
    {
        return set_parse_error(error, "qubit_array", "Missing required key 'gate_noise_model'");
    }

    if (!seen.single_gate_delay_ns)
    {
        return set_parse_error(error, "qubit_array", "Missing required key 'single_gate_delay_ns'");
    }

    return true;
}

bool parse_qubit_arrays(const std::vector<ParsedLine>& lines,
                        std::size_t& index,
                        int parent_indent,
                        std::vector<QubitArrayConfig>& qubit_arrays,
                        std::string& error) noexcept
{
    while (index < lines.size() && lines[index].indent > parent_indent)
    {
        QubitArrayConfig array_config{
            .num_qubits = 0,
            .qubit_decoherence_noise_model = make_default_qubit_noise_model(),
            .single_gate_delay_ns = 0,
            .gate_noise_model = make_default_gate_noise_model()};

        if (!parse_single_qubit_array_item(lines, index, parent_indent, array_config, error))
        {
            return false;
        }

        qubit_arrays.push_back(array_config);
    }

    if (qubit_arrays.empty())
    {
        return set_parse_error(error, "qubit_arrays", "List must not be empty");
    }

    return true;
}

bool parse_quantum_operation_delays(const std::vector<ParsedLine>& lines,
                                    std::size_t& index,
                                    int parent_indent,
                                    QuantumOperationsDelaysNs& delays,
                                    std::string& error) noexcept
{
    while (index < lines.size() && lines[index].indent > parent_indent)
    {
        if (lines[index].indent != parent_indent + kIndentStep)
        {
            return set_parse_error_near(error,
                                        "quantum_operations_delay_ns",
                                        "Invalid indentation",
                                        lines[index].content);
        }

        const auto key_value = config_parse::split_key_value(lines[index].content);
        if (!key_value.has_value())
        {
            return set_parse_error_near(error,
                                        "quantum_operations_delay_ns",
                                        "Invalid key/value",
                                        lines[index].content);
        }

        uint64_t parsed_delay = 0;
        if (!config_parse::parse_non_negative_u64(key_value->second, parsed_delay))
        {
            return set_parse_error(error,
                                   "quantum_operations_delay_ns",
                                   "Invalid value for '" + key_value->first + "': '" + key_value->second + "'");
        }

        if (key_value->first == kTwoQubitGateRotationKey)
        {
            delays.two_gate_delay = parsed_delay;
        }
        else if (key_value->first == kMeasurementKey)
        {
            delays.measurement_delay = parsed_delay;
        }
        else if (key_value->first == kEntanglementKey)
        {
            delays.entanglement_delay = parsed_delay;
        }
        else if (key_value->first == kQubitMoveKey)
        {
            delays.qubit_move_delay = parsed_delay;
        }
        else if (key_value->first == kQubitSwapKey)
        {
            delays.qubit_swap_delay = parsed_delay;
        }
        else if (key_value->first == kQubitInitKey)
        {
            delays.qubit_init_delay = parsed_delay;
        }
        else
        {
            return set_parse_error(error,
                                   "quantum_operations_delay_ns",
                                   "Unknown key '" + key_value->first + "'");
        }

        ++index;
    }

    return true;
}

bool parse_system_section(const std::vector<ParsedLine>& lines,
                          std::size_t& index,
                          int parent_indent,
                          SystemConfig& system,
                          std::string& error) noexcept
{
    while (index < lines.size() && lines[index].indent > parent_indent)
    {
        if (lines[index].indent != parent_indent + kIndentStep)
        {
            return set_parse_error_near(error, "system", "Invalid indentation", lines[index].content);
        }

        const auto key_value = config_parse::split_key_value(lines[index].content);
        if (!key_value.has_value())
        {
            return set_parse_error_near(error, "system", "Invalid key/value", lines[index].content);
        }

        if (key_value->first == kTransmissionCycleTimeKey)
        {
            float parsed_value = 0.0f;
            if (!config_parse::parse_non_negative_float(key_value->second, parsed_value))
            {
                return set_parse_error(error,
                                       "system",
                                       "Invalid value for '" + key_value->first + "': '" + key_value->second + "'");
            }

            system.transmission_cycle_time_us = parsed_value;
        }
        else if (key_value->first == kQubitNoiseEnabledKey)
        {
            bool parsed_value = false;
            if (!config_parse::parse_bool(key_value->second, parsed_value))
            {
                return set_parse_error(error,
                                       "system",
                                       "Invalid value for '" + key_value->first + "': '" + key_value->second + "'");
            }

            system.qubit_noise_enabled = parsed_value;
        }
        else if (key_value->first == kEntanglementNoiseEnabledKey)
        {
            bool parsed_value = false;
            if (!config_parse::parse_bool(key_value->second, parsed_value))
            {
                return set_parse_error(error,
                                       "system",
                                       "Invalid value for '" + key_value->first + "': '" + key_value->second + "'");
            }

            system.entanglement_noise_enabled = parsed_value;
        }
        else
        {
            return set_parse_error(error, "system", "Unknown key '" + key_value->first + "'");
        }

        ++index;
    }

    return true;
}

bool parse_node_field(const std::vector<ParsedLine>& lines,
                      std::size_t& index,
                      int item_indent,
                      NodeConfig& node_config,
                      bool& has_qubit_arrays,
                      std::string& error) noexcept
{
    const auto key_value = config_parse::split_key_value(lines[index].content);
    if (!key_value.has_value())
    {
        return set_parse_error_near(error, "node", "Invalid key/value", lines[index].content);
    }

    if (key_value->first == kNameKey)
    {
        node_config.name = key_value->second;
        ++index;
        return true;
    }

    if (key_value->first == kNodeTypeKey)
    {
        const auto parsed_type = config_parse::parse_node_type(key_value->second);
        if (!parsed_type.has_value())
        {
            return set_parse_error(error, "node", "Unsupported node_type '" + key_value->second + "'");
        }

        node_config.node_type = *parsed_type;
        ++index;
        return true;
    }

    if (key_value->first == kTwoQubitGateNoiseModelKey)
    {
        if (!key_value->second.empty())
        {
            return set_parse_error(error, "node", "two_qubit_gate_noise_model must be a nested map");
        }

        ++index;
        if (!parse_model_block(lines,
                               index,
                               item_indent + kIndentStep,
                               config_parse::two_qubit_gate_noise_model_parsers(),
                               node_config.two_qubit_gate_noise_model.config_data,
                               error))
        {
            return false;
        }

        return true;
    }

    if (key_value->first == kQubitArraysKey)
    {
        if (!key_value->second.empty())
        {
            return set_parse_error(error, "node", "qubit_arrays must be a list");
        }

        ++index;
        if (!parse_qubit_arrays(lines, index, item_indent + kIndentStep, node_config.qubit_arrays, error))
        {
            return false;
        }

        has_qubit_arrays = true;
        return true;
    }

    if (key_value->first == kQuantumOperationsDelays)
    {
        if (!key_value->second.empty())
        {
            return set_parse_error(error, "node", "quantum_operations_delay_ns must be a map");
        }

        ++index;
        if (!parse_quantum_operation_delays(lines,
                                            index,
                                            item_indent + kIndentStep,
                                            node_config.qubit_operations_delays,
                                            error))
        {
            return false;
        }

        return true;
    }

    return set_parse_error(error, "node", "Unknown key '" + key_value->first + "'");
}

bool finalize_node_config(NodeConfig& node_config,
                          bool has_qubit_arrays,
                          std::string& error) noexcept
{
    if (!has_qubit_arrays)
    {
        return set_parse_error(error, "node", "Requires qubit_arrays");
    }

    int sum_qubits = 0;
    for (const auto& array_config : node_config.qubit_arrays)
    {
        sum_qubits += array_config.num_qubits;
    }

    node_config.num_qubits = sum_qubits;

    return true;
}

bool parse_single_node_item(const std::vector<ParsedLine>& lines,
                            std::size_t& index,
                            NodeConfig& node_config,
                            std::string& error) noexcept
{
    if (lines[index].indent != kIndentStep)
    {
        return set_parse_error_near(error, "nodes", "Invalid indentation", lines[index].content);
    }

    if (lines[index].content.empty() || lines[index].content[0] != '-')
    {
        return set_parse_error(error, "nodes", "Expected list item");
    }

    const int item_indent = lines[index].indent;
    const std::string inline_content = config_parse::trim(lines[index].content.substr(1));
    ++index;

    bool has_qubit_arrays = false;

    if (!inline_content.empty())
    {
        const auto inline_kv = config_parse::split_key_value(inline_content);
        if (!inline_kv.has_value())
        {
            return set_parse_error(error, "node", "Invalid inline key/value '" + inline_content + "'");
        }

        if (inline_kv->first == kNameKey)
        {
            node_config.name = inline_kv->second;
        }
        else if (inline_kv->first == kNodeTypeKey)
        {
            const auto parsed_type = config_parse::parse_node_type(inline_kv->second);
            if (!parsed_type.has_value())
            {
                return set_parse_error(error, "node", "Unsupported node_type '" + inline_kv->second + "'");
            }

            node_config.node_type = *parsed_type;
        }
        else if (inline_kv->first == kQubitArraysKey)
        {
            if (!inline_kv->second.empty())
            {
                return set_parse_error(error, "node", "qubit_arrays must be a list");
            }

            if (!parse_qubit_arrays(lines, index, item_indent + kIndentStep, node_config.qubit_arrays, error))
            {
                return false;
            }

            has_qubit_arrays = true;
        }
        else
        {
            return set_parse_error(error,
                                   "node",
                                   "Unsupported inline item '" + inline_content + "'");
        }
    }

    while (index < lines.size() && lines[index].indent > item_indent)
    {
        if (lines[index].indent != item_indent + kIndentStep)
        {
            return set_parse_error_near(error, "node", "Invalid indentation", lines[index].content);
        }

        if (!parse_node_field(lines, index, item_indent, node_config, has_qubit_arrays, error))
        {
            return false;
        }
    }

    return finalize_node_config(node_config, has_qubit_arrays, error);
}

bool parse_nodes(const std::vector<ParsedLine>& lines,
                 std::size_t& index,
                 AppConfig& app_config,
                 std::string& error) noexcept
{
    while (index < lines.size() && lines[index].indent > 0)
    {
        NodeConfig node_config{
            .num_qubits = 0,
            .name = "",
            .node_type = QuantumNodeType::GENERIC,
            .qubit_arrays = {},
            .two_qubit_gate_noise_model = make_default_gate_noise_model()};

        if (!parse_single_node_item(lines, index, node_config, error))
        {
            return false;
        }

        app_config.nodes.push_back(node_config);
    }

    return true;
}

bool parse_entanglement_link_field(const std::vector<ParsedLine>& lines,
                                   std::size_t& index,
                                   int item_indent,
                                   EntanglementLinkConfig& link_config,
                                   bool& has_node_a,
                                   bool& has_node_b,
                                   bool& has_attempt_model,
                                   bool& has_link_noise_model,
                                   std::string& error) noexcept
{
    const auto key_value = config_parse::split_key_value(lines[index].content);
    if (!key_value.has_value())
    {
        return set_parse_error_near(error, "entanglement_link", "Invalid key/value", lines[index].content);
    }

    if (key_value->first == kNodeAKey)
    {
        if (key_value->second.empty())
        {
            return set_parse_error(error, "entanglement_link", "node_a must not be empty");
        }

        link_config.node_a = key_value->second;
        has_node_a = true;
        ++index;
        return true;
    }

    if (key_value->first == kNodeBKey)
    {
        if (key_value->second.empty())
        {
            return set_parse_error(error, "entanglement_link", "node_b must not be empty");
        }

        link_config.node_b = key_value->second;
        has_node_b = true;
        ++index;
        return true;
    }

    if (key_value->first == kAttemptModelKey)
    {
        if (!key_value->second.empty())
        {
            return set_parse_error(error, "entanglement_link", "attempt_model must be a nested map");
        }

        ++index;
        if (!parse_model_block(lines,
                               index,
                               item_indent + kIndentStep,
                               config_parse::entanglement_attempt_model_parsers(),
                               link_config.attempt_model.config_data,
                               error))
        {
            return false;
        }

        has_attempt_model = true;
        return true;
    }

    if (key_value->first == kLinkNoiseModelKey)
    {
        if (!key_value->second.empty())
        {
            return set_parse_error(error, "entanglement_link", "link_noise_model must be a nested map");
        }

        ++index;
        if (!parse_model_block(lines,
                               index,
                               item_indent + kIndentStep,
                               config_parse::entanglement_link_noise_model_parsers(),
                               link_config.link_noise_model.config_data,
                               error))
        {
            return false;
        }

        has_link_noise_model = true;
        return true;
    }

    return set_parse_error(error, "entanglement_link", "Unknown key '" + key_value->first + "'");
}

bool parse_single_entanglement_link_item(const std::vector<ParsedLine>& lines,
                                         std::size_t& index,
                                         int parent_indent,
                                         EntanglementLinkConfig& link_config,
                                         std::string& error) noexcept
{
    if (lines[index].indent != parent_indent + kIndentStep)
    {
        return set_parse_error_near(error, "entanglement_links", "Invalid indentation", lines[index].content);
    }

    if (lines[index].content.empty() || lines[index].content[0] != '-')
    {
        return set_parse_error(error, "entanglement_links", "Expected list entry under entanglement_links");
    }

    const int item_indent = lines[index].indent;
    const std::string inline_content = config_parse::trim(lines[index].content.substr(1));
    ++index;

    bool has_node_a = false;
    bool has_node_b = false;
    bool has_attempt_model = false;
    bool has_link_noise_model = false;

    if (!inline_content.empty())
    {
        const auto inline_kv = config_parse::split_key_value(inline_content);
        if (!inline_kv.has_value())
        {
            return set_parse_error(error, "entanglement_link", "Invalid inline key/value '" + inline_content + "'");
        }

        if (inline_kv->first == kNodeAKey)
        {
            if (inline_kv->second.empty())
            {
                return set_parse_error(error, "entanglement_link", "node_a must not be empty");
            }

            link_config.node_a = inline_kv->second;
            has_node_a = true;
        }
        else if (inline_kv->first == kNodeBKey)
        {
            if (inline_kv->second.empty())
            {
                return set_parse_error(error, "entanglement_link", "node_b must not be empty");
            }

            link_config.node_b = inline_kv->second;
            has_node_b = true;
        }
        else
        {
            return set_parse_error(error,
                                   "entanglement_link",
                                   "Unsupported inline item '" + inline_content + "' (expected 'node_a' or 'node_b')");
        }
    }

    while (index < lines.size() && lines[index].indent > item_indent)
    {
        if (lines[index].indent != item_indent + kIndentStep)
        {
            return set_parse_error_near(error, "entanglement_link", "Invalid indentation", lines[index].content);
        }

        if (!parse_entanglement_link_field(lines,
                                           index,
                                           item_indent,
                                           link_config,
                                           has_node_a,
                                           has_node_b,
                                           has_attempt_model,
                                           has_link_noise_model,
                                           error))
        {
            return false;
        }
    }

    if (!has_node_a)
    {
        return set_parse_error(error, "entanglement_link", "Missing required key 'node_a'");
    }

    if (!has_node_b)
    {
        return set_parse_error(error, "entanglement_link", "Missing required key 'node_b'");
    }

    if (!has_attempt_model)
    {
        return set_parse_error(error, "entanglement_link", "Missing required key 'attempt_model'");
    }

    if (!has_link_noise_model)
    {
        return set_parse_error(error, "entanglement_link", "Missing required key 'link_noise_model'");
    }

    return true;
}

bool parse_entanglement_links(const std::vector<ParsedLine>& lines,
                              std::size_t& index,
                              int parent_indent,
                              std::vector<EntanglementLinkConfig>& entanglement_links,
                              std::string& error) noexcept
{
    while (index < lines.size() && lines[index].indent > parent_indent)
    {
        EntanglementLinkConfig link_config{};

        if (!parse_single_entanglement_link_item(lines, index, parent_indent, link_config, error))
        {
            return false;
        }

        entanglement_links.push_back(link_config);
    }

    // Unlike qubit_arrays/nodes, an empty or entirely absent list is valid --
    // not every deployment needs entanglement noise configured on every link.
    return true;
}

} // namespace

std::string get_default_config_path() noexcept {
    const char* explicit_path = std::getenv("QILAB_CONFIG_PATH");
    if (explicit_path && explicit_path[0] != '\0') {
        return std::string(explicit_path);
    }

    const char* config_dir = std::getenv("QILAB_CONFIG_DIR");
    if (config_dir && config_dir[0] != '\0') {
        return std::string(config_dir) + "/nodes.yaml";
    }

    const char* home = std::getenv("HOME");
    if (!home) {
        return ".qilab/config/nodes.yaml";
    }
    return std::string(home) + "/.qilab/config/nodes.yaml";
}

bool create_default_config() noexcept {
    std::string config_path = get_default_config_path();
    std::filesystem::path path(config_path);

    std::filesystem::path dir = path.parent_path();
    std::error_code error;
    if (!std::filesystem::exists(dir, error)) {
        if (error) {
            LOG_ERROR("Failed to inspect config directory '" + dir.string() + "': " + error.message());
            return false;
        }

        if (!std::filesystem::create_directories(dir, error)) {
            if (error) {
                LOG_ERROR("Failed to create config directory '" + dir.string() + "': " + error.message());
                return false;
            }
        } else {
            LOG_INFO("Created config directory: " + dir.string());
        }
    }

    if (std::filesystem::exists(config_path, error)) {
        if (error) {
            LOG_ERROR("Failed to inspect config file '" + config_path + "': " + error.message());
            return false;
        }

        LOG_INFO("Config file already exists at: " + config_path);
        return true;
    }

    if (error) {
        LOG_ERROR("Failed to inspect config file '" + config_path + "': " + error.message());
        return false;
    }

    std::ofstream config_file(config_path);
    if (!config_file.is_open()) {
        LOG_ERROR("Failed to create config file: " + config_path);
        return false;
    }

    // Convention in the generated file: '##' marks explanatory prose, a
    // single '#' marks commented-out CONFIG placed immediately before the
    // line's own YAML indentation. Uncommenting a block is therefore exactly
    // "delete the first '#'" on each single-'#' line.
    config_file << "## QILab Node Configuration\n";
    config_file << "## This file defines the quantum network nodes for simulation\n";
    config_file << "\n";
    config_file << "## QiLab System Configuration\n";
    config_file << "system:\n";
    config_file << "  transmission_cycle_time_us: 5\n";
    config_file << "  qubit_noise_enabled: true\n";
    config_file << "  entanglement_noise_enabled: true\n";
    config_file << "\n";
    config_file << "nodes:\n";
    config_file << "  - name: node0\n";
    config_file << "    node_type: GENERIC\n";
    config_file << "    two_qubit_gate_noise_model:\n";
    config_file << "      type: DEPOLARIZING\n";
    config_file << "      depolarizing_probability: 0.0\n";
    config_file << "    quantum_operations_delay_ns:\n";
    config_file << "      two_qubit_gate_rotation: 0\n";
    config_file << "      measurement: 0\n";
    config_file << "      entanglement: 0\n";
    config_file << "      qubit_move: 0\n";
    config_file << "      qubit_swap: 0\n";
    config_file << "      qubit_init: 0\n";
    config_file << "    qubit_arrays:\n";
    config_file << "      - num_qubits: 10\n";
    config_file << "        single_gate_delay_ns: 0\n";
    config_file << "        qubit_decoherence_noise_model:\n";
    config_file << "          type: BASIC\n";
    config_file << "          t2_value: 1000\n";
    config_file << "        gate_noise_model:\n";
    config_file << "          type: DEPOLARIZING\n";
    config_file << "          depolarizing_probability: 0.0\n";
    config_file << "\n";
    // Entanglement needs two nodes, so node1 and the link that joins it are
    // emitted commented out: uncomment BOTH to get a working entangled setup.
    config_file << "## Uncomment node1 AND entanglement_links below together to run a\n";
    config_file << "## two-node network with one entangled link.\n";
    config_file << "#  - name: node1\n";
    config_file << "#    node_type: GENERIC\n";
    config_file << "#    two_qubit_gate_noise_model:\n";
    config_file << "#      type: DEPOLARIZING\n";
    config_file << "#      depolarizing_probability: 0.0\n";
    config_file << "#    quantum_operations_delay_ns:\n";
    config_file << "#      two_qubit_gate_rotation: 0\n";
    config_file << "#      measurement: 0\n";
    config_file << "#      entanglement: 0\n";
    config_file << "#      qubit_move: 0\n";
    config_file << "#      qubit_swap: 0\n";
    config_file << "#      qubit_init: 0\n";
    config_file << "#    qubit_arrays:\n";
    config_file << "#      - num_qubits: 10\n";
    config_file << "#        single_gate_delay_ns: 0\n";
    config_file << "#        qubit_decoherence_noise_model:\n";
    config_file << "#          type: BASIC\n";
    config_file << "#          t2_value: 1000\n";
    config_file << "#        gate_noise_model:\n";
    config_file << "#          type: DEPOLARIZING\n";
    config_file << "#          depolarizing_probability: 0.0\n";
    config_file << "\n";
    config_file << "## One entry per physical link, keyed by node names above.\n";
    config_file << "#entanglement_links:\n";
    config_file << "#  - node_a: node0\n";
    config_file << "#    node_b: node1\n";
    config_file << "#    attempt_model:\n";
    config_file << "#      type: BASIC\n";
    config_file << "#      attempt_duration_ns: 1000\n";
    config_file << "#      batch_size: 100\n";
    config_file << "#      p_success: 0.5\n";
    config_file << "#      rng_seed: 42\n";
    config_file << "#    link_noise_model:\n";
    config_file << "#      type: BASIC\n";
    config_file << "#      p_depolarizing: 0.0\n";
    config_file << "#      p_dephasing: 0.0\n";

    config_file.close();
    LOG_INFO("Created default config file at: " + config_path);
    return true;
}

std::optional<AppConfig> load_config(const std::string& config_path) noexcept {
    const std::string path = config_path.empty() ? get_default_config_path() : config_path;

    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        if (error) {
            LOG_ERROR("Failed to inspect config file '" + path + "': " + error.message());
            return std::nullopt;
        }

        LOG_INFO("Config file not found. Creating default configuration...");
        if (!create_default_config()) {
            return std::nullopt;
        }
    }

    std::ifstream config_file(path);
    if (!config_file.is_open()) {
        LOG_ERROR("Failed to open config file: " + path);
        return std::nullopt;
    }

    const std::vector<ParsedLine> lines = read_config_lines(config_file);

    AppConfig app_config;

    bool found_nodes = false;

    std::size_t index = 0;
    while (index < lines.size())
    {
        if (lines[index].indent != 0)
        {
            LOG_ERROR("Unexpected indentation near top-level entry '" + lines[index].content + "'");
            return std::nullopt;
        }

        const auto key_value = config_parse::split_key_value(lines[index].content);
        if (!key_value.has_value())
        {
            LOG_ERROR("Invalid top-level key/value entry: " + lines[index].content);
            return std::nullopt;
        }

        if (key_value->first == kNodesKey)
        {
            if (!key_value->second.empty())
            {
                LOG_ERROR("'nodes' must be a list block");
                return std::nullopt;
            }

            ++index;
            std::string parse_error;
            if (!parse_nodes(lines, index, app_config, parse_error))
            {
                LOG_ERROR("nodes parsing error: " + parse_error);
                return std::nullopt;
            }

            found_nodes = true;
            continue;
        }

        if (key_value->first == kEntanglementLinksKey)
        {
            if (!key_value->second.empty())
            {
                LOG_ERROR("'entanglement_links' must be a list block");
                return std::nullopt;
            }

            ++index;
            std::string parse_error;
            if (!parse_entanglement_links(lines, index, 0, app_config.entanglement_links, parse_error))
            {
                LOG_ERROR("entanglement_links parsing error: " + parse_error);
                return std::nullopt;
            }

            continue;
        }

        if (key_value->first == kSystemKey)
        {
            if (!key_value->second.empty())
            {
                LOG_ERROR("'system' must be a map block");
                return std::nullopt;
            }

            ++index;
            std::string parse_error;
            if (!parse_system_section(lines, index, 0, app_config.system, parse_error))
            {
                LOG_ERROR("system parsing error: " + parse_error);
                return std::nullopt;
            }

            continue;
        }

        LOG_ERROR("Unknown top-level key: " + key_value->first);
        return std::nullopt;
    }

    if (!found_nodes || app_config.nodes.empty())
    {
        LOG_ERROR("Config must contain a non-empty 'nodes' section");
        return std::nullopt;
    }

    std::unordered_set<std::string> node_names;
    for (const auto& node : app_config.nodes)
    {
        node_names.insert(node.name);
    }

    std::unordered_set<std::string> seen_links;
    for (const auto& link : app_config.entanglement_links)
    {
        if (link.node_a == link.node_b)
        {
            LOG_ERROR("entanglement_links entry has identical node_a and node_b: '" + link.node_a + "'");
            return std::nullopt;
        }

        if (node_names.find(link.node_a) == node_names.end())
        {
            LOG_ERROR("entanglement_links entry references unknown node_a '" + link.node_a + "'");
            return std::nullopt;
        }

        if (node_names.find(link.node_b) == node_names.end())
        {
            LOG_ERROR("entanglement_links entry references unknown node_b '" + link.node_b + "'");
            return std::nullopt;
        }

        const std::string canonical_key = (link.node_a < link.node_b)
            ? link.node_a + "|" + link.node_b
            : link.node_b + "|" + link.node_a;
        if (!seen_links.insert(canonical_key).second)
        {
            LOG_ERROR("Duplicate entanglement_links entry for link (" + link.node_a + ", " + link.node_b + ")");
            return std::nullopt;
        }
    }

    LOG_INFO("Loaded configuration with " + std::to_string(app_config.nodes.size()) + " nodes from: " + path);
    return app_config;
}
