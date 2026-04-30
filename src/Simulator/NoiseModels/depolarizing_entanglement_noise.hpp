/*
** Created on: 29.04.2026
** Created by: GitHub Copilot
** Description: Contains the depolarizing entanglement noise model for the simulator
*/

#pragma once
#include <cmath>

#include "Simulator/entanglement_noise_interface.hpp"

struct DepolarizingEntanglementNoiseConfig
{
    float error_rate;   // Error rate for entanglement operation in range [0, 1]
    float gate_time_ns; // Reference entanglement operation duration in nanoseconds
};

class DepolarizingEntanglementNoiseModel : public EntanglementNoiseInterface
{
public:
    DepolarizingEntanglementNoiseModel(const DepolarizingEntanglementNoiseConfig& config)
        : error_rate(config.error_rate), gate_time_ns(config.gate_time_ns)
    {
    }

    ~DepolarizingEntanglementNoiseModel() override = default;

    inline float calculate_entanglement_success_probability() override
    {
        if (error_rate <= 0.0f)
        {
            return 1.0f;
        }

        if (error_rate >= 1.0f)
        {
            return 0.0f;
        }

        return 1.0f - error_rate;
    }

    inline float calculate_entanglement_fidelity() override
    {
        if (gate_time_ns <= 0.0f)
        {
            return 0.0f;
        }

        const float fidelity = 1.0f - (error_rate * 0.5f);
        if (fidelity < 0.0f)
        {
            return 0.0f;
        }

        return fidelity;
    }

private:
    float error_rate;
    float gate_time_ns;
};
