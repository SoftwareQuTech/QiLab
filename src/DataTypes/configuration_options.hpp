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
#include <string>
#include <vector>

#include "Simulator/NoiseModels/QubitDecoherenceNoise/qubit_noise_basic.hpp"
#include "Simulator/NoiseModels/QubitDecoherenceNoise/phase_flip_noise.hpp"
#include "Simulator/NoiseModels/EntanglementGenerationNoise/entanglement_attempt_noise.hpp"
#include "Simulator/NoiseModels/EntanglementGenerationNoise/entanglement_channel_noise.hpp"

enum class QuantumNodeType : uint8_t
{
    GENERIC = 0,
    NV_CENTER = 1,
    ION_TRAP = 2,
    PROCESSOR = 3,
};

enum class QubitNoiseModelType : uint8_t
{
    BASIC = 0,
    ADVANCED = 1,
    PHASE_FLIP = 2,
};

using QubitNoiseConfigData = std::variant<
    QubitNoiseBasicConfig,
    QubitNoiseAdvancedConfig,
    PhaseFlipNoiseConfig
>;

using EntanglementAttemptConfigData = std::variant<
    EntanglementAttemptBasicConfig
>;

using EntanglementChannelConfigData = std::variant<
    EntanglementChannelBasicConfig
>;

struct QubitNoiseModelConfig
{
    QubitNoiseConfigData config_data;
};

struct EntanglementAttemptModelConfig
{
    EntanglementAttemptConfigData config_data;
};

struct EntanglementChannelModelConfig
{
    EntanglementChannelConfigData config_data;
};

// One physical link between two configured nodes (identified by name), each
// carrying its own attempt model (entanglement manager) and channel model
// (simulator manager). node_a/node_b must match names in AppConfig::nodes --
// validated at config-load time.
struct EntanglementLinkConfig
{
    std::string node_a;
    std::string node_b;
    EntanglementAttemptModelConfig attempt_model;
    EntanglementChannelModelConfig channel_model;
};

struct QubitArrayConfig
{
    int num_qubits;                  // number of qubits in this array
    QubitNoiseModelConfig noise_model;
};

struct QuantumOperationsDelaysNs
{
    uint64_t single_gate_delay;
    uint64_t two_gate_delay;
    uint64_t measurement_delay;
    uint64_t entanglement_delay;
    uint64_t qubit_move_delay;
    uint64_t qubit_swap_delay;
    uint64_t qubit_init_delay;
};

struct NodeConfig
{
    int num_qubits;                                    // derived: sum of qubit_arrays[].num_qubits
    std::string name;
    QuantumNodeType node_type = QuantumNodeType::GENERIC;
    QuantumOperationsDelaysNs qubit_operations_delays;
    std::vector<QubitArrayConfig> qubit_arrays;
};

/// "QiLab System Configuration" -- global runtime parameters (optional section, defaults apply)
struct SystemConfig
{
    float transmission_cycle_time_us = 5.0f;    // transmission runner cycle period, microseconds
    bool  qubit_noise_enabled = true;           // gate qubit-in-memory noise application in the sim manager
    bool  entanglement_noise_enabled = true;    // gate entanglement-channel noise application in the sim manager
};