// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

/**
 * @file message_broker.hpp
 * @brief Thread-safe message broker for inter-runner communication.
 *
 * This header defines the message routing infrastructure for communication between
 * different runner components (CommandRunner, SimRunner, EntanglementRunner).
 * It implements a message routing system with validation of valid communication paths.
 */

#pragma once
#include <atomic>
#include <cstdint>
#include <memory>

#include "DataTypes/commands.hpp"
#include "DataTypes/responses.hpp"
#include "MessageBroker/thread_safe_ring_buffer.hpp"

/**
 * @brief Enumeration of system component types.
 *
 * Identifies the different runner types that participate in message exchange.
 * - SimRunner: Handles simulation execution
 * - CommandRunner: Processes and routes commands
 * - EntanglementRunner: Manages entanglement operations
 */
enum class DestinationType {
    SimRunner,
    CommandRunner,
    EntanglementRunner
};

/**
 * @brief Represents a message route with source and destination.
 *
 * Validates that communication paths follow allowed patterns:
 * - Commands can flow from CommandRunner to SimRunner or EntanglementRunner
 * - Commands can flow from EntanglementRunner to SimRunner
 * - Responses can flow from EntanglementRunner to CommandRunner
 * - Responses can flow from SimRunner to CommandRunner
 */
struct MessageRoute {
    DestinationType source;      ///< Source component sending the message
    DestinationType destination; ///< Destination component receiving the message
    
    /**
     * @brief Validates if a command route is allowed.
     *
     * @return true if the source-to-destination combination is valid for commands
     * @return false if the route is not allowed
     */
    bool is_valid_cmd_route() const noexcept {
        // Define valid routes
        if (source == DestinationType::CommandRunner) {
            return destination == DestinationType::SimRunner ||
                   destination == DestinationType::EntanglementRunner;
        }
        if (source == DestinationType::EntanglementRunner) {
            return destination == DestinationType::SimRunner;
        }
        return false;
    }

    /**
     * @brief Validates if a response route is allowed.
     *
     * @return true if the source-to-destination combination is valid for responses
     * @return false if the route is not allowed
     */
    bool is_valid_rsp_route() const noexcept
    {
        if (source == DestinationType::EntanglementRunner)
        {
            return destination == DestinationType::CommandRunner;
        }
        if (source == DestinationType::SimRunner)
        {
            return destination == DestinationType::CommandRunner;
        }
        return false;
    }
};

/**
 * @brief Template wrapper for transporting typed data with routing information.
 *
 * Combines a message route with the actual payload data and node identifier.
 * Used for both command and response transport.
 *
 * @tparam T The payload type (CommandBase or ResponseData)
 */
template <typename T>
struct TransportData : MessageRoute {
    std::unique_ptr<T> transport; ///< The actual message payload
    uint16_t node_id;             ///< Identifier for the target node
};

/**
 * @brief Thread-safe message broker for inter-component communication.
 *
 * Implements a publish-subscribe pattern using lock-free ring buffers for
 * different message types. The broker:
 * - Routes commands to the appropriate runner (Sim or Entanglement)
 * - Routes responses back to the CommandRunner
 * - Validates all routes against predefined communication policies
 * - Uses cache-aligned buffers to minimize false sharing in multithreaded context
 *
 * The design uses three separate queues:
 * - sim_commands_: Commands destined for the simulation runner
 * - ent_commands_: Commands destined for the entanglement runner
 * - cmd_responses_: Responses destined for the command runner
 *
 * @note All methods are noexcept and thread-safe for use in real-time contexts
 * @note Ring buffer capacity is fixed at 256 messages per queue
 */
class MessageBroker {
private:
    static constexpr size_t QUEUE_SIZE = 256; ///< Capacity of each message queue

    /// Commands destined for simulation runner (cache-aligned to prevent false sharing)
    alignas(64) ThreadSafeRingBuffer<TransportData<CommandBase>, QUEUE_SIZE> sim_commands_;

    /// Commands destined for entanglement runner (cache-aligned to prevent false sharing)
    alignas(64) ThreadSafeRingBuffer<TransportData<CommandBase>, QUEUE_SIZE> ent_commands_;

    /// Responses destined for command runner (cache-aligned to prevent false sharing)
    alignas(64) ThreadSafeRingBuffer<TransportData<ResponseData>, QUEUE_SIZE> cmd_responses_;

public:
    /**
     * @brief Routes a command to the appropriate runner based on its destination.
     *
     * Validates the command route against valid routing policies before queueing.
     * Uses move semantics to transfer ownership of the command data.
     *
     * @param cmd Command transport object with destination and payload
     * @return true if the command was successfully queued
     * @return false if the route is invalid or the queue is full
     *
     * @note This is a lock-free operation suitable for high-frequency use
     */
    bool route_command(TransportData<CommandBase>&& cmd) noexcept;

    /**
     * @brief Routes a response back to the CommandRunner.
     *
     * Validates the response route against valid routing policies before queueing.
     * Responses can only originate from SimRunner or EntanglementRunner and must
     * be destined for CommandRunner.
     *
     * @param resp Response transport object with destination and payload
     * @return true if the response was successfully queued
     * @return false if the route is invalid or the queue is full
     *
     * @note This is a lock-free operation suitable for high-frequency use
     */
    bool route_response(TransportData<ResponseData>&& resp) noexcept;

    /**
     * @brief Retrieves a command for the specified runner type.
     *
     * Dequeues a command from the appropriate queue based on the requesting
     * runner type. This allows each runner to independently pull work from
     * its designated queue without blocking other runners.
     *
     * @param cmd Output parameter that will receive the command if available
     * @param runner_type The type of runner requesting a command
     *                   (determines which queue to poll)
     * @return true if a command was available and retrieved
     * @return false if no command was available in the appropriate queue
     *
     * @note This is a lock-free operation suitable for real-time polling loops
     */
    bool get_command(TransportData<CommandBase> &cmd, DestinationType runner_type) noexcept;

    /**
     * @brief Retrieves a response for the CommandRunner.
     *
     * Dequeues a response from the command response queue. Only the
     * CommandRunner should call this method.
     *
     * @param resp Output parameter that will receive the response if available
     * @return true if a response was available and retrieved
     * @return false if no response was available
     *
     * @note This is a lock-free operation suitable for real-time polling loops
     */
    bool get_response(TransportData<ResponseData>& resp) noexcept;
};


