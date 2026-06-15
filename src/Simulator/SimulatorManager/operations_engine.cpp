#include <chrono>

#include "operations_engine.hpp"
#include "logger/logger_api.hpp"

template <typename T>
void swap_pop_erase(std::vector<T> &vec, std::size_t idx)
{
    if (idx != vec.size() - 1)
    {
        vec[idx] = std::move(vec.back());
    }
    vec.pop_back();
}

OperationsEngine::OperationsEngine()
{
    waiting_responses.reserve(MAX_RESPONSE_BUFFER_SIZE);
}


bool OperationsEngine::add_new_response_to_wait(ResponseData response, uint16_t node_id, uint64_t wait_ns)
{
    if(wait_ns > 1000000000) // 1 second, just a sanity check to avoid overflow and very long waits
    {
        LOG_WARNING("Attempting to add a response with an unusually long wait time (" + std::to_string(wait_ns) + " ns). Check if this is intentional.");
    }
    
    if (waiting_responses.size() >= waiting_responses.capacity())
    {
        LOG_WARNING("OperationsEngine response buffer is full. Allocating more space, but will cause reallocation.");
        waiting_responses.reserve(waiting_responses.size() + MAX_RESPONSE_BUFFER_SIZE); // Increase capacity to avoid future warnings, but this will cause a reallocation
    }


    auto current_time_ns = std::chrono::steady_clock::now();
    waiting_responses.push_back(Entry{std::move(response), node_id, current_time_ns + std::chrono::nanoseconds(wait_ns)});
    PROF_START(prof_event_operation_time_wait);
    return true;
}

bool OperationsEngine::try_get_ready_response(ResponseData& response, uint16_t &node_id)
{
    auto current_time_ns = std::chrono::steady_clock::now();
    for (size_t i = 0; i < waiting_responses.size(); ++i)
    {
        if (waiting_responses[i].ready_time_ns <= current_time_ns)
        {
            response = std::move(waiting_responses[i].response);
            node_id = waiting_responses[i].node_id;
            swap_pop_erase(waiting_responses, i);
            PROF_STOP(prof_event_operation_time_wait);
            return true;
        }
    }
    return false;
}