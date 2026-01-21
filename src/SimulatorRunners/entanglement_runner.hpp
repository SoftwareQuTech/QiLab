/**
 * @file entanglement_runner.hpp
 * @brief Manages quantum entanglement coordination between nodes.
 *
 * The EntanglementRunner handles the complex task of coordinating entanglement
 * operations between quantum nodes. It acts as the interface between the command
 * system and the EntanglementManager, processing entanglement requests and
 * distributing results back through the message broker.
 */

/*
** Created on: 23.09.2025
** Created by: mrkolev
** Description: Contains entanglement manager. This is a module
**              that is supposed to handle actual sync between nodes
**              and simulate the CR check.
*/

#pragma once
#include <cstdint>
#include <memory>

#include <MessageBroker/message_broker.hpp>
#include "DataTypes/command_data.hpp"
#include "Simulator/entanglement/entanglement_manager.hpp"

/**
 * @brief Runs the entanglement coordination loop on a dedicated thread.
 *
 * EntanglementRunner acts as the bridge between incoming entanglement commands and
 * the EntanglementManager. It processes the following workflow:
 *
 * 1. **Command Reception**: Polls the message broker for entanglement commands
 *    (ENTANGLEMENT_ATTEMPT or ENTANGLEMENT_MEASURE)
 * 2. **Request Processing**: Extracts neighbor node information and passes requests
 *    to the EntanglementManager
 * 3. **Manager Coordination**: Allows EntanglementManager to match and schedule
 *    paired entanglement operations
 * 4. **Result Distribution**: Routes responses and resulting commands back via the
 *    message broker using callbacks
 *
 * ### Command Processing
 * The runner handles two types of entanglement commands:
 * - ENTANGLEMENT_ATTEMPT: Initial request to entangle with a neighbor
 * - ENTANGLEMENT_MEASURE: Measurement-based entanglement operation
 *
 * Invalid entanglement commands are logged and skipped.
 *
 * ### Callback Model
 * The EntanglementManager uses two callbacks to communicate results:
 * - Response callback: Invoked when a request fails (timeout)
 * - Command callback: Invoked when matched pairs execute their entanglement operations
 *
 * Both callbacks route their results through the message broker to appropriate
 * destinations (CommandRunner or SimRunner).
 *
 * @note Runs on a dedicated thread with optional CPU core affinity
 * @note Thread-safe interaction with message broker
 * @note Non-copyable and non-movable by design
 */
class EntanglementRunner
{
    public:
    /**
     * @brief Constructs an EntanglementRunner with a message broker.
     *
     * Initializes the EntanglementManager with callback functions that route
     * responses and commands through the message broker.
     *
     * @param mb Shared pointer to the MessageBroker for command/response routing
     */
    EntanglementRunner(std::shared_ptr<MessageBroker> mb);

    /**
     * @brief Destructor (defaulted).
     */
    ~EntanglementRunner() = default;

    /**
     * @brief Start the entanglement runner thread.
     *
     * Begins the main entanglement coordination loop which:
     * - Polls the message broker for entanglement commands
     * - Extracts neighbor node identifiers from commands
     * - Passes requests to the EntanglementManager for matching and execution
     * - Periodically calls update_requests() on the manager
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
     * @brief Signals the entanglement runner to stop.
     *
     * Sets the abort flag which causes the main loop in on_run() to exit.
     * This is a non-blocking call; the actual thread termination may take
     * a short time to complete.
     */
    void on_abort();

    protected:
    /**
     * @brief Routes an entanglement failure response back to the requesting node.
     *
     * Callback invoked by EntanglementManager when an entanglement request
     * times out without finding a matching request from the neighbor.
     * The response is routed to the CommandRunner via the message broker.
     *
     * @param node_id The node that made the failed entanglement request
     * @param status The failure status (typically ENTANGLEMENT_SYNC_FAILURE)
     */
    void send_response_async(int node_id, Outcome_Status_t status);

    /**
     * @brief Routes a matched entanglement command to the destination node.
     *
     * Callback invoked by EntanglementManager when a matched entanglement pair
     * reaches execution time. The command is routed to the appropriate destination
     * (typically SimRunner) via the message broker.
     *
     * @param node_id The destination node for this entanglement command
     * @param cmd The entanglement command to execute (moved)
     */
    void send_command_async(int node_id, std::unique_ptr<CommandBase> cmd);

    private:
    /// Reference to the message broker for command/response routing
    std::shared_ptr<MessageBroker> message_broker;

    /// Manager responsible for matching and coordinating entanglement requests
    std::unique_ptr<EntanglementManager> entanglement_manager;

    /// Flag to signal thread termination
    bool abort = false;
    
    /// Non-copyable
    EntanglementRunner(const EntanglementRunner&) = delete;

    /// Non-movable
    EntanglementRunner(const EntanglementRunner&&) = delete;

    /// Non-assignable
    EntanglementRunner& operator=(EntanglementRunner&) = delete;

    /// Non-move-assignable
    EntanglementRunner& operator=(EntanglementRunner&&) = delete;
};