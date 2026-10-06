// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

#pragma once

#include <string>

#include "ConfigurationEngine/parse_scalar_helpers.hpp"
#include "DataTypes/configuration_options.hpp"

namespace config_parse {

inline bool parse_qubit_advanced_model(const FieldMap& fields,
                                       QubitNoiseConfigData& output,
                                       std::string& error) noexcept
{
    if (!ensure_allowed_keys(fields, {"type", "t1_value", "t2_value", "gate_error_rate"}, error))
    {
        return false;
    }

    std::string t1_text;
    std::string t2_text;
    std::string gate_error_text;
    if (!require_key(fields, "t1_value", t1_text, error) ||
        !require_key(fields, "t2_value", t2_text, error) ||
        !require_key(fields, "gate_error_rate", gate_error_text, error))
    {
        return false;
    }

    float t1_value = 0.0f;
    float t2_value = 0.0f;
    float gate_error_rate = 0.0f;
    if (!parse_non_negative_float(t1_text, t1_value) ||
        !parse_non_negative_float(t2_text, t2_value) ||
        !parse_non_negative_float(gate_error_text, gate_error_rate))
    {
        error = "Invalid value in ADVANCED qubit model";
        return false;
    }

    output = QubitNoiseAdvancedConfig{
        .t1_value = t1_value,
        .t2_value = t2_value,
        .gate_error_rate = gate_error_rate};
    return true;
}

} // namespace config_parse
