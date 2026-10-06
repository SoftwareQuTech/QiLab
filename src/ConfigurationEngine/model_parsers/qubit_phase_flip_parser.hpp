// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

#pragma once

#include <string>

#include "ConfigurationEngine/parse_scalar_helpers.hpp"
#include "DataTypes/configuration_options.hpp"

namespace config_parse {

inline bool parse_qubit_phase_flip_model(const FieldMap& fields,
                                         QubitNoiseConfigData& output,
                                         std::string& error) noexcept
{
    if (!ensure_allowed_keys(fields, {"type", "t2_value"}, error))
    {
        return false;
    }

    std::string t2_text;
    if (!require_key(fields, "t2_value", t2_text, error))
    {
        return false;
    }

    float t2_value = 0.0f;
    if (!parse_non_negative_float(t2_text, t2_value))
    {
        error = "Invalid value for 't2_value': " + t2_text;
        return false;
    }

    output = PhaseFlipNoiseConfig{.t2_value = t2_value};
    LOG_INFO("Parsed PHASE_FLIP qubit noise model with t2_value = " + std::to_string(t2_value) + " ns.");
    return true;
}

} // namespace config_parse
