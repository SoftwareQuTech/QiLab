// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

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
#include "ConfigurationEngine/model_parsers/gate_noise_depolarizing_parser.hpp"
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

using GateNoiseModelParser = bool (*)(const FieldMap&, GateNoiseConfigData&, std::string&) noexcept;

// Two registries, because the single- and two-qubit depolarizing channels accept
// different maximum probabilities (0.75 vs 0.9375) and are configured at
// different granularities: per qubit array vs per node (Sec. 2.1).
inline const std::unordered_map<std::string, GateNoiseModelParser>& single_qubit_gate_noise_model_parsers() noexcept
{
    static const std::unordered_map<std::string, GateNoiseModelParser> parsers = {
        {"DEPOLARIZING", &parse_single_qubit_gate_noise_depolarizing_model},
    };

    return parsers;
}

inline const std::unordered_map<std::string, GateNoiseModelParser>& two_qubit_gate_noise_model_parsers() noexcept
{
    static const std::unordered_map<std::string, GateNoiseModelParser> parsers = {
        {"DEPOLARIZING", &parse_two_qubit_gate_noise_depolarizing_model},
    };

    return parsers;
}

} // namespace config_parse



