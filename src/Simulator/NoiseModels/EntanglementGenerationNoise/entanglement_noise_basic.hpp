/*
** Created on: 20.04.2026
** Created by: mrkolev
** Description: Contains the basic qubit noise model for the simulator
*/

#pragma once
#include <cstdint>
#include <chrono>
#include <cmath>

#include "Simulator/entanglement_noise_interface.hpp"

struct EntanglementNoiseBasicConfig
{
    float t2_value; // Coherence time in nanoseconds
};

class EntanglementNoiseBasicModel : public EntanglementNoiseInterface
{
public:
    EntanglementNoiseBasicModel() {}
    ~EntanglementNoiseBasicModel() override = default;

    inline float calculate_entanglement_success_probability() override
    {
        return 0.0f; // Placeholder value, replace with actual calculation logic
    }

    inline float calculate_entanglement_fidelity() override
    {
        return 0.0f; // Placeholder value, replace with actual calculation logic
    }
};
