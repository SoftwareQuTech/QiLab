/*
** Created on: 29.04.2026
** Created by: GitHub Copilot
** Description: Contains the phase-flip qubit noise model for the simulator
*/

#pragma once
#include <chrono>

#include "Simulator/qubit_noise_interface.hpp"

struct PhaseFlipNoiseConfig
{
    float flip_probability; // Flip probability per gate operation in range [0, 1]
};

class PhaseFlipQubitModel : public QubitNoiseInterface
{
public:
    PhaseFlipQubitModel(const PhaseFlipNoiseConfig& config)
        : flip_probability(config.flip_probability),
          last_operation_timestamp_ns(std::chrono::steady_clock::now())
    {
    }

    ~PhaseFlipQubitModel() override = default;

    inline int mark_touched(uint64_t delay = 0) override
    {
        last_operation_timestamp_ns = std::chrono::steady_clock::now() + std::chrono::nanoseconds(delay);
        return 0;
    }

    inline float get_qubit_decoherence_probability() override
    {
        if (flip_probability <= 0.0f)
        {
            return 0.0f;
        }

        if (flip_probability >= 1.0f)
        {
            return 1.0f;
        }

        return flip_probability;
    }

private:
    std::chrono::time_point<std::chrono::steady_clock> last_operation_timestamp_ns;
    float flip_probability;
};
