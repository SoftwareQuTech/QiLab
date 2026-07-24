#pragma once

#include <string>
#include <unordered_map>

#include "ConfigurationEngine/model_parsers/entanglement_attempt_basic_parser.hpp"
#include "ConfigurationEngine/model_parsers/entanglement_channel_basic_parser.hpp"
#include "ConfigurationEngine/parse_scalar_helpers.hpp"
#include "DataTypes/configuration_options.hpp"

namespace config_parse {

using EntanglementAttemptModelParser = bool (*)(const FieldMap&, EntanglementAttemptConfigData&, std::string&) noexcept;
using EntanglementChannelModelParser = bool (*)(const FieldMap&, EntanglementChannelConfigData&, std::string&) noexcept;

inline const std::unordered_map<std::string, EntanglementAttemptModelParser>& entanglement_attempt_model_parsers() noexcept
{
    static const std::unordered_map<std::string, EntanglementAttemptModelParser> parsers = {
        {"BASIC", &parse_entanglement_attempt_basic_model},
    };

    return parsers;
}

inline const std::unordered_map<std::string, EntanglementChannelModelParser>& entanglement_channel_model_parsers() noexcept
{
    static const std::unordered_map<std::string, EntanglementChannelModelParser> parsers = {
        {"BASIC", &parse_entanglement_channel_basic_model},
    };

    return parsers;
}

} // namespace config_parse
