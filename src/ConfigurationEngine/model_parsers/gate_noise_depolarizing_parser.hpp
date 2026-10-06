// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

#pragma once

#include <string>

#include "ConfigurationEngine/parse_scalar_helpers.hpp"
#include "DataTypes/configuration_options.hpp"

namespace config_parse {

namespace detail {

// The single- and two-qubit depolarizing channels differ only in their maximal
// mixing point, so both parsers share this body. The bound comes from the model
// class itself rather than a literal, so the parser, the model, and the schema
// cannot drift apart.
inline bool parse_gate_noise_depolarizing_fields(const FieldMap& fields,
                                                float max_depolarizing_probability,
                                                GateNoiseConfigData& output,
                                                std::string& error) noexcept
{
    if (!ensure_allowed_keys(fields, {"type", "depolarizing_probability"}, error))
    {
        return false;
    }

    std::string depolarizing_probability_text;
    if (!require_key(fields, "depolarizing_probability", depolarizing_probability_text, error))
    {
        return false;
    }

    float depolarizing_probability = 0.0f;
    if (!parse_non_negative_float(depolarizing_probability_text, depolarizing_probability) ||
        depolarizing_probability > max_depolarizing_probability)
    {
        error = "Invalid value for 'depolarizing_probability': " + depolarizing_probability_text +
                " (must be in [0, " + std::to_string(max_depolarizing_probability) +
                "], the channel's maximal mixing point)";
        return false;
    }

    output = GateNoiseDepolarizingConfig{.depolarizing_probability = depolarizing_probability};
    return true;
}

} // namespace detail

// Single-qubit gate depolarizing noise, configured per qubit array.
inline bool parse_single_qubit_gate_noise_depolarizing_model(const FieldMap& fields,
                                                            GateNoiseConfigData& output,
                                                            std::string& error) noexcept
{
    return detail::parse_gate_noise_depolarizing_fields(
        fields, SingleQubitGateNoiseModel::MAX_DEPOLARIZING_PROBABILITY, output, error);
}

// Two-qubit gate depolarizing noise, configured per node.
inline bool parse_two_qubit_gate_noise_depolarizing_model(const FieldMap& fields,
                                                          GateNoiseConfigData& output,
                                                          std::string& error) noexcept
{
    return detail::parse_gate_noise_depolarizing_fields(
        fields, TwoQubitGateNoiseModel::MAX_DEPOLARIZING_PROBABILITY, output, error);
}

} // namespace config_parse
