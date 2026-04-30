#pragma once

#include <string>

#include "ConfigurationEngine/parse_scalar_helpers.hpp"
#include "DataTypes/configuration_options.hpp"

namespace config_parse {

inline bool parse_qubit_phase_flip_model(const FieldMap& fields,
                                         QubitNoiseConfigData& output,
                                         std::string& error) noexcept
{
    if (!ensure_allowed_keys(fields, {"type", "flip_probability"}, error))
    {
        return false;
    }

    std::string probability_text;
    if (!require_key(fields, "flip_probability", probability_text, error))
    {
        return false;
    }

    float flip_probability = 0.0f;
    if (!parse_non_negative_float(probability_text, flip_probability))
    {
        error = "Invalid value for 'flip_probability': " + probability_text;
        return false;
    }

    output = PhaseFlipNoiseConfig{.flip_probability = flip_probability};
    return true;
}

} // namespace config_parse
