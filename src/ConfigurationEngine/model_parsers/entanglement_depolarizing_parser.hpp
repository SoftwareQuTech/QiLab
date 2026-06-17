#pragma once

#include <string>

#include "ConfigurationEngine/parse_scalar_helpers.hpp"
#include "DataTypes/configuration_options.hpp"

namespace config_parse {

inline bool parse_entanglement_depolarizing_model(const FieldMap& fields,
                                                  EntanglementNoiseConfigData& output,
                                                  std::string& error) noexcept
{
    if (!ensure_allowed_keys(fields, {"type", "error_rate", "gate_time_ns"}, error))
    {
        return false;
    }

    std::string error_rate_text;
    std::string gate_time_text;
    if (!require_key(fields, "error_rate", error_rate_text, error) ||
        !require_key(fields, "gate_time_ns", gate_time_text, error))
    {
        return false;
    }

    float error_rate = 0.0f;
    float gate_time_ns = 0.0f;
    if (!parse_non_negative_float(error_rate_text, error_rate) ||
        !parse_non_negative_float(gate_time_text, gate_time_ns))
    {
        error = "Invalid value in DEPOLARIZING entanglement model";
        return false;
    }

    output = DepolarizingEntanglementNoiseConfig{.error_rate = error_rate, .gate_time_ns = gate_time_ns};
    return true;
}

} // namespace config_parse
