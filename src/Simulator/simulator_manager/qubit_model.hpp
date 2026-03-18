/*
* 
*/

#pragma once
#include <cstdint>
#include <chrono>
#include <cmath>

class QubitModel
{
public:
    QubitModel(){}
    QubitModel(const float& t2_value) : t2(t2_value), last_operation_timestamp_ns(std::chrono::steady_clock::now()){}
    ~QubitModel();

    inline int mark_touched()
    {
        // Update the timestamp of the last operation
        last_operation_timestamp_ns = std::chrono::steady_clock::now();
    }   
    
    inline float get_qubit_decoherence_probability()
    {
        auto now = std::chrono::steady_clock::now();
        auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now - last_operation_timestamp_ns).count();
        return 1.0f - std::exp(-static_cast<float>(elapsed_ns) / t2);
    }

private:
    std::chrono::time_point<std::chrono::steady_clock> last_operation_timestamp_ns;
    float t2;
};
