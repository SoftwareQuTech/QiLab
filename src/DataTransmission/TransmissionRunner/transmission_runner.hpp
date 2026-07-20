/*
** Created on: 08.09.2025
** Created by: mrkolev
** Description: Data transmission runner - manages command serialization, transfer, and response deserialization
*/

#pragma once
#include <chrono>
#include <functional>

#include "DataTypes/command_data.hpp"
#include "DataTypes/responses.hpp"
#include "DataTransmission/TransmissionProcessor/transmission_processor.hpp"
#include "DataTypes/commands.hpp"
#include "logger/profiler_api.hpp"
#include "MessageBroker/message_broker.hpp"

/**
 * @brief Manages the data transmission pipeline for registered nodes.
 *
 * DataTransmissionRunner is responsible for the complete transmission cycle:
 * 1. Serializing commands received from the message broker
 * 2. Transmitting serialized data via the TransmissionProcessor
 * 3. Deserializing responses from the transmission medium
 * 4. Publishing responses back to the message broker
 *
 * This class runs on a dedicated thread and can be pinned to a specific CPU core
 * for improved performance isolation and cache locality.
 *
 * The runner maintains vectors of commands and responses for all registered nodes,
 * pre-allocating memory to minimize dynamic allocations in the main loop.
 *
 * @note Thread-safe communication with the message broker is managed through the
 *       MessageBroker interface.
 * @note Performance metrics are collected for serialization, transfer, and 
 *       deserialization phases via the Profiler interface.
 */
class DataTransmissionRunner
{
    public:
    /**
     * @brief Constructs a DataTransmissionRunner instance.
     *
     * @param nodes                 Vector of node identifiers to manage transmission for.
     * @param processor             Unique pointer to TransmissionProcessor for handling
     *                              low-level transmission operations.
     * @param broker                Shared pointer to MessageBroker for command/response
     *                              exchange with other components.
     * @param cycle_time_us         Transmission cycle period in microseconds (from
     *                              system.transmission_cycle_time_us config), default 5.0.
     */
    DataTransmissionRunner(std::vector<int16_t> nodes, std::unique_ptr<TransmissionProcessor> processor, std::shared_ptr<MessageBroker> broker, float cycle_time_us = 5.0f);
    
    /**
     * @brief Destructor.
     *
     * Cleans up any allocated resources.
     */
    ~DataTransmissionRunner();

    /**
     * @brief Start the transmission runner thread.
     * 
     * Begins the main transmission loop which continuously:
     * - Retrieves commands from the message broker for registered nodes
     * - Serializes commands into transport format
     * - Transfers data via the transmission processor
     * - Deserializes responses
     * - Publishes responses back to the message broker
     *
     * The thread will continue running until on_abort() is called.
     * 
     * @param core_id The CPU core to pin this thread to. Use -1 to disable core affinity
     *                and let the OS scheduler handle thread placement.
     * 
     * @return Status code indicating success or error conditions.
     */
    int on_run(int core_id = -1);

    /**
     * @brief Signals the transmission runner to stop.
     *
     * Sets the abort flag which causes the main loop in on_run() to exit.
     * This is a non-blocking call; the actual thread termination may take
     * a short time to complete.
     */
    void on_abort();

    private:
    /// Message broker for publishing/subscribing to commands and responses
    std::shared_ptr<MessageBroker> message_broker;
    
    /// Identifiers of nodes this runner manages transmission for
    std::vector<int16_t> registered_nodes;
    
    /// Per-node transmission state data (cycle counter and LP data)
    std::vector<NodeTransmissionData> node_transmission_data;
    
    /// Flag to signal thread termination
    bool abort = false;
    
    /// Processor for low-level transmission operations
    std::unique_ptr<TransmissionProcessor> transmission_processor;

    /// Transmission loop cycle period, from system.transmission_cycle_time_us config
    std::chrono::nanoseconds transmission_cycle_time;

    /// Profiling event IDs for performance monitoring
    
    /// Total time spent in the handler thread main loop
    Profiler_id prof_event_handler_thread_timing = PROF_REGISTER_EVENT("handler_thread_timing");
    
    /// Time spent serializing commands to transport format
    Profiler_id prof_event_serialize = PROF_REGISTER_EVENT("handler_thread_serialize");
    
    /// Time spent transferring data over the transmission medium
    Profiler_id prof_event_spi_transfer = PROF_REGISTER_EVENT("handler_thread_transfer");
    
    /// Time spent deserializing responses from transport format
    Profiler_id prof_event_deserialize = PROF_REGISTER_EVENT("handler_thread_deserialize");

};