/*
** Created on: 29.04.2026
** Created by: GitHub Copilot
** Description: Contains the phase-flip qubit noise model for the simulator
*/

#pragma once
#include <chrono>
#include <cmath>

#include "Simulator/qubit_noise_interface.hpp"
#include "logger/logger_api.hpp"


struct PhaseFlipNoiseConfig
{
    float t2_value; // T2 time of the qubit in ns
};

class PhaseFlipQubitModel : public QubitNoiseInterface
{
public:
    PhaseFlipQubitModel(const PhaseFlipNoiseConfig& config)
        : t2(config.t2_value),
          last_operation_timestamp_ns(std::chrono::steady_clock::now()),
          elapsed_time_ns(0)
    {
    }

    ~PhaseFlipQubitModel() override = default;

    inline int mark_touched() override
    {
        last_operation_timestamp_ns = std::chrono::steady_clock::now();
        return 0;
    }

    inline float get_qubit_decoherence_probability() override
    // get the probability of a phase flip error occurring based on the elapsed time 
    // since the last operation and the T2 time of the qubit
    {
        if (t2 < 0.0f)
        {
            LOG_ERROR("$$ get_qubit_decoherence_probability: T2 value is invalid.");
            return 0.0f;
        }

        // LOG_INFO("$$ get_qubit_decoherence_probability: Calculating phase flip error probability based on elapsed time and T2 value.");

        auto now = std::chrono::steady_clock::now();
        auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now - last_operation_timestamp_ns).count();
        
        return 0.5f * (1.0 - std::exp(-(static_cast<float>(elapsed_ns) / t2)));
    }


private:
    std::chrono::time_point<std::chrono::steady_clock> last_operation_timestamp_ns;
    float t2;
    float elapsed_time_ns; 
};
