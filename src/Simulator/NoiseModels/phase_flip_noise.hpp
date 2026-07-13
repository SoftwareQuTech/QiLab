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
          elapsed_time_ns(0),
          total_elapsed_sim_time_ns(0),
          times_called_counter(0)
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
        times_called_counter++;
        if (times_called_counter % 5 > 3)
        {
            return 0.0f;
        }
        if (t2 < 0.0f)
        {
            LOG_ERROR("$$ get_qubit_decoherence_probability: T2 value is invalid.");
            return 0.0f;
        }

        LOG_INFO("$$ get_qubit_decoherence_probability: Calculating phase flip error probability based on elapsed time and T2 value.");

        auto now = std::chrono::steady_clock::now();
        auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now - last_operation_timestamp_ns).count();
        total_elapsed_sim_time_ns += elapsed_ns;
        LOG_INFO("$$ get_qubit_decoherence_probability: Total_elapsed_sim_time_ns: " + std::to_string(total_elapsed_sim_time_ns) + " ns");
        // LOG_INFO("$$ Time elapsed since last operation: " + std::to_string(elapsed_ns) + " ns");

        // qubit_survival = exp(- (elapsed_time / t2)^2)
        // rho = (1 - flip_prob)rho + flip_prob * Z * rho * Z

        // 1 - 2p_flip = W 
        // p_flip = (1 - W) / 2

        // return 0.5f * (1.0 - std::exp(-(static_cast<float>(elapsed_ns) / t2) * (static_cast<float>(elapsed_ns) / t2)));;
        auto flip_prob = 0.5f * (1.0 - std::exp(-(static_cast<float>(elapsed_ns) / t2)));
        if (flip_prob > 0.5f)
        {
            LOG_WARNING("$$ get_qubit_decoherence_probability: Phase flip error probability is high: " + std::to_string(flip_prob));
            return 0.5f;
        }
        return flip_prob;
    }


private:
    std::chrono::time_point<std::chrono::steady_clock> last_operation_timestamp_ns;
    float t2;
    float elapsed_time_ns;
    float total_elapsed_sim_time_ns;
    int times_called_counter;
};
