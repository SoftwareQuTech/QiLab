#pragma once

#include <string>

#include "ConfigurationEngine/parse_scalar_helpers.hpp"
#include "DataTypes/configuration_options.hpp"

namespace config_parse {

inline bool parse_qubit_amplitude_damping_model(const FieldMap& fields,
                                                QubitNoiseConfigData& output,
                                                std::string& error) noexcept
{
    if (!ensure_allowed_keys(fields, {"type", "t1_ns", "t2_ns"}, error))
    {
        return false;
    }

    std::string t1_text;
    std::string t2_text;
    if (!require_key(fields, "t1_ns", t1_text, error) ||
        !require_key(fields, "t2_ns", t2_text, error))
    {
        return false;
    }

    float t1_ns = 0.0f;
    float t2_ns = 0.0f;
    if (!parse_non_negative_float(t1_text, t1_ns) ||
        !parse_non_negative_float(t2_text, t2_ns))
    {
        error = "Invalid value in AMPLITUDE_DAMPING qubit model";
        return false;
    }

    output = AmplitudeDampingNoiseConfig{.t1_ns = t1_ns, .t2_ns = t2_ns};
    return true;
}

} // namespace config_parse
