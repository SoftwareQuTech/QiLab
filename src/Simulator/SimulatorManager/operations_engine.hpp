/*
 * operations_engine.hpp
 *
 *  Created on: May 10, 2026
 *      Author: Milen Kolev
 *      Description: This module contains the implementation of the operations engine, which is responsible for applying the configured delays for each operation type.
 */

#pragma once
#include <cstdint>
#include <vector>
#include <chrono>

#include "DataTypes/responses.hpp"
#include "logger/profiler_api.hpp"
class OperationsEngine
{

public:
    OperationsEngine();

    bool add_new_response_to_wait(ResponseData response, uint16_t node_id, uint64_t wait_ns);
    bool try_get_ready_response(ResponseData& response, uint16_t &node_id);
private:
    struct Entry
    {
        ResponseData response;
        uint16_t node_id;
        std::chrono::steady_clock::time_point ready_time_ns; // Absolute time in nanoseconds when the response will be ready
    };

    // the amount of responses that can be buffered in the ring buffer, should be enough for most cases, and can be adjusted if needed
    static constexpr uint16_t MAX_RESPONSE_BUFFER_SIZE = 64; 
    std::vector<Entry> waiting_responses; // Vector to hold responses that are waiting for their delay to elapse

    Profiler_id prof_event_operation_time_wait = PROF_REGISTER_EVENT("operation_time_wait"); 
};
