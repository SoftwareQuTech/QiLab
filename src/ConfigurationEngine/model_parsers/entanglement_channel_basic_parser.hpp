#pragma once

#include <string>

#include "ConfigurationEngine/parse_scalar_helpers.hpp"
#include "DataTypes/configuration_options.hpp"

namespace config_parse {

inline bool parse_entanglement_channel_basic_model(const FieldMap& fields,
                                                    EntanglementChannelConfigData& output,
                                                    std::string& error) noexcept
{
    if (!ensure_allowed_keys(fields, {"type", "p_depolarizing", "p_dephasing"}, error))
    {
        return false;
    }

    std::string p_depolarizing_text;
    if (!require_key(fields, "p_depolarizing", p_depolarizing_text, error))
    {
        return false;
    }

    float p_depolarizing = 0.0f;
    if (!parse_non_negative_float(p_depolarizing_text, p_depolarizing) || p_depolarizing > 1.0f)
    {
        error = "Invalid value for 'p_depolarizing': " + p_depolarizing_text + " (must be in [0, 1])";
        return false;
    }

    std::string p_dephasing_text;
    if (!require_key(fields, "p_dephasing", p_dephasing_text, error))
    {
        return false;
    }

    float p_dephasing = 0.0f;
    if (!parse_non_negative_float(p_dephasing_text, p_dephasing) || p_dephasing > 1.0f)
    {
        error = "Invalid value for 'p_dephasing': " + p_dephasing_text + " (must be in [0, 1])";
        return false;
    }

    output = EntanglementChannelBasicConfig{.p_depolarizing = p_depolarizing, .p_dephasing = p_dephasing};
    return true;
}

} // namespace config_parse
