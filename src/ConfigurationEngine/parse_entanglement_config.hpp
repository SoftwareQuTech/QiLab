#pragma once

#include <optional>
#include <string>
#include <unordered_map>

#include "ConfigurationEngine/model_parsers/entanglement_basic_parser.hpp"
#include "ConfigurationEngine/parse_scalar_helpers.hpp"
#include "DataTypes/configuration_options.hpp"

namespace config_parse {

using EntanglementModelParser = bool (*)(const FieldMap&, EntanglementNoiseConfigData&, std::string&) noexcept;

inline const std::unordered_map<std::string, EntanglementModelParser>& entanglement_model_parsers() noexcept
{
    static const std::unordered_map<std::string, EntanglementModelParser> parsers = {
        {"BASIC", &parse_entanglement_basic_model},
    };

    return parsers;
}

} // namespace config_parse
