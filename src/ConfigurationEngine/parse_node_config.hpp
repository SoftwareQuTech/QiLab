/*
    * parse_node_config.hpp
    *
    *  Created on: Jun 19, 2024
    * Author: mrkolev
    * Description: Contains functions to parse node configuration from YAML files.
*/

#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

#include "ConfigurationEngine/parse_scalar_helpers.hpp"
#include "ConfigurationEngine/model_parsers/qubit_advanced_parser.hpp"
#include "ConfigurationEngine/model_parsers/qubit_basic_parser.hpp"
#include "ConfigurationEngine/model_parsers/qubit_phase_flip_parser.hpp"
#include "DataTypes/configuration_options.hpp"

namespace config_parse {

inline std::optional<QuantumNodeType> parse_node_type(const std::string& text) noexcept
{
    const std::string trimmed = trim(text);
    if (trimmed == "GENERIC")
    {
        return QuantumNodeType::GENERIC;
    }

    if (trimmed == "NV_CENTER")
    {
        return QuantumNodeType::NV_CENTER;
    }

    if (trimmed == "ION_TRAP")
    {
        return QuantumNodeType::ION_TRAP;
    }

    if (trimmed == "PROCESSOR")
    {
        return QuantumNodeType::PROCESSOR;
    }

    return std::nullopt;
}

using QubitModelParser = bool (*)(const FieldMap&, QubitNoiseConfigData&, std::string&) noexcept;

inline const std::unordered_map<std::string, QubitModelParser>& qubit_model_parsers() noexcept
{
    static const std::unordered_map<std::string, QubitModelParser> parsers = {
        {"BASIC", &parse_qubit_basic_model},
        {"ADVANCED", &parse_qubit_advanced_model},
        {"PHASE_FLIP", &parse_qubit_phase_flip_model},
    };

    return parsers;
}

} // namespace config_parse



