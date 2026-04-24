/*
    * configuration_options.hpp
    *
    *  Created on: Jun 19, 2024
    *  Author: mrkolev
    * Description: Contains the configuration options for the application. This includes
    * the structure of the configuration file and the corresponding C++ structures to hold the configuration data.
    * The configuration file is expected to be in YAML format and should define the quantum nodes and their properties (e.g., number of qubits).
*/

#pragma once
#include <cstdint>
#include <variant>

#include "Simulator/NoiseModels/qubit_noise_basic.hpp"
#include "Simulator/NoiseModels/entanglement_noise_basic.hpp"

enum class QuantumNodeType : uint8_t
{
    GENERIC = 0,
    NV_CENTER = 1,
    ION_TRAP = 2,
}; 

enum class QubitNoiseModelType : uint8_t
{
    BASIC = 0,
    ADVANCED = 1,
};

enum class EntanglementNoiseModelType : uint8_t
{
    BASIC = 0,
    ADVANCED = 1,
};

using QubitNoiseConfigData = std::variant<QubitNoiseBasicConfig, QubitNoiseAdvancedConfig>;
using EntanglementNoiseConfigData = std::variant<EntanglementNoiseBasicConfig>;

struct QubitNoiseModelConfig
{
    QubitNoiseConfigData config_data;
};

struct EntanglementNoiseModelConfig
{
    EntanglementNoiseConfigData config_data;
};

struct NodeConfig
{
    int num_qubits;
    QuantumNodeType node_type = QuantumNodeType::GENERIC;
    QubitNoiseModelConfig qubit_noise_model;
};