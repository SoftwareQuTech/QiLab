/*
** Created on: 29.04.2026
** Created by: GitHub Copilot
** Description: Contains the depolarizing qubit noise model for the simulator
*/

#pragma once
#include <chrono>
#include <cmath>

#include "Simulator/qubit_noise_interface.hpp"

struct DepolarizingQubitNoiseConfig
{
    float error_rate;   // Error rate per gate operation in range [0, 1]
    float gate_time_ns; // Reference gate duration in nanoseconds
};

class DepolarizingQubitModel : public QubitNoiseInterface
{
public:
    DepolarizingQubitModel(const DepolarizingQubitNoiseConfig& config)
        : error_rate(config.error_rate),
          gate_time_ns(config.gate_time_ns),
          last_operation_timestamp_ns(std::chrono::steady_clock::now())
    {
    }

    ~DepolarizingQubitModel() override = default;

    inline int mark_touched() override
    {
        last_operation_timestamp_ns = std::chrono::steady_clock::now();
        return 0;
    }

    inline float get_qubit_decoherence_probability() override
    {
        if (gate_time_ns <= 0.0f || error_rate <= 0.0f)
        {
            return 0.0f;
        }

        auto now = std::chrono::steady_clock::now();
        auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            now - last_operation_timestamp_ns).count();

        const float hazard = error_rate / gate_time_ns;
        const float probability = 1.0f - std::exp(-hazard * static_cast<float>(elapsed_ns));

        if (probability < 0.0f)
        {
            return 0.0f;
        }

        if (probability > 1.0f)
        {
            return 1.0f;
        }

        return probability;
    }

private:
    std::chrono::time_point<std::chrono::steady_clock> last_operation_timestamp_ns;
    float error_rate;
    float gate_time_ns;
};
