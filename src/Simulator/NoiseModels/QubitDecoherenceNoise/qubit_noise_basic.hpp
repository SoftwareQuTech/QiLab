/*
** Created on: 20.04.2026
** Created by: mrkolev
** Description: Contains the basic qubit noise model for the simulator
*/

#pragma once
#include <cstdint>
#include <chrono>
#include <cmath>

#include "Simulator/qubit_noise_interface.hpp"

struct QubitNoiseBasicConfig
{
    float t2_value; // Coherence time in nanoseconds
};

struct QubitNoiseAdvancedConfig
{
    float t1_value; // Relaxation time in nanoseconds
    float t2_value; // Coherence time in nanoseconds
    float gate_error_rate; // Error rate per gate operation
};

class QubitModelBasic : public QubitNoiseInterface
{
public:
    QubitModelBasic(const QubitNoiseBasicConfig& config) : t2(config.t2_value), last_operation_timestamp_ns(std::chrono::steady_clock::now()){}
    ~QubitModelBasic() override = default;

    inline int mark_touched(uint64_t delay = 0) override
    {
        // Update the timestamp of the last operation
        last_operation_timestamp_ns = std::chrono::steady_clock::now() + std::chrono::nanoseconds(delay);
        return 0; // Return value to satisfy the interface, can be adjusted as needed
    }   
    
    inline float get_qubit_decoherence_probability() override
    {
        auto now = std::chrono::steady_clock::now();
        auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now - last_operation_timestamp_ns).count();
        return 1.0f - std::exp(-static_cast<float>(elapsed_ns) / t2);
    }

private:
    std::chrono::time_point<std::chrono::steady_clock> last_operation_timestamp_ns;
    float t2;
};
