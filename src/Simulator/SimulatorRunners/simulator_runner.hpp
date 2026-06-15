/**
 * @file simulator_runner.hpp
 * @brief Simulation runner that executes quantum operations on simulated qubits.
 *
 * The SimulatorRunner manages command dispatch to a quantum simulator, handling
 * qubit allocation, command execution, and performance profiling of all operations.
 * It runs on a dedicated thread and coordinates with the message broker for
 * inter-component communication.
 */

/*
** Created on: 25.08.2025
** Created by: mrkolev
** Description: Contains simulator runner code. It handles the commands
**              and gives them to the simulator
*/

#pragma once
#include <vector>

#include "Simulator/sim_interface.hpp"
#include "logger/profiler_api.hpp"
#include "DataTypes/command_data.hpp"
#include "MessageBroker/message_broker.hpp"
#include "Simulator/SimulatorManager/simulator_manager.hpp"
/**
 * @brief Executes quantum simulation operations on a dedicated thread.
 *
 * SimulatorRunner serves as the bridge between the command system and the underlying
 * quantum simulator. Key responsibilities:
 *
 * 1. **Node Management**: Register/unregister quantum nodes with allocated qubit capacity
 * 2. **Command Execution**: Dispatch commands from the message broker to the simulator
 * 3. **Qubit Allocation**: Manage qubit address space with per-node offsets
 * 4. **Performance Monitoring**: Profile all operation types via Profiler interface
 * 5. **Thread Management**: Run on a dedicated thread pinned to a specific CPU core
 *
 * ### Node Registration Model
 * Nodes are registered sequentially with an assigned number of qubits. The runner
 * maintains a qubit offset for each node to map node-relative qubit indices to
 * global qubit indices in the simulator. This allows multiple nodes to operate
 * independently while sharing the same simulator instance.
 *
 * ### Command Processing Flow
 * 1. Runner polls message broker for commands destined to SimRunner
 * 2. Executes command against the simulator with appropriate qubit offset
 * 3. Sends response back via message broker
 * 4. Profiles operation for performance analysis
 *
 * ### Resource Constraints
 * - Maximum 4 nodes can be registered
 * - Maximum 200 total qubits available
 * - These limits prevent excessive simulator overhead
 *
 * @note Thread-safe access to simulator via message broker
 * @note All operations are extensively profiled for performance analysis
 * @note Non-copyable and non-movable by design
 */
class SimulatorRunner
{
    public:
    /**
     * @brief Constructs a SimulatorRunner with a simulator and message broker.
     *
     * @param sim Unique pointer to the SimulatorManager implementation to use for operations
     * @param broker Shared pointer to the MessageBroker for command/response routing
     */
    SimulatorRunner(std::unique_ptr<SimulatorManager> sim_manager, std::shared_ptr<MessageBroker> broker);

    /**
     * @brief Destructor (defaulted).
     */
    ~SimulatorRunner() = default;

    /**
     * @brief Start the simulator runner thread.
     *
     * Begins the main simulation loop which continuously:
     * - Polls the message broker for commands destined to SimRunner
     * - Executes commands against the simulator
     * - Sends responses back via the message broker
     * - Profiles each operation type
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
     * @brief Signals the simulator runner to stop.
     *
     * Sets the abort flag which causes the main loop in on_run() to exit.
     * This is a non-blocking call; the actual thread termination may take
     * a short time to complete.
     */
    void on_abort();

    private:
    /// Reference to the message broker for command/response routing
    std::shared_ptr<MessageBroker> message_broker;

    /// Manager for handling simulator operations and state
    std::unique_ptr<SimulatorManager> simulator_manager;

    /// Flag to signal thread termination
    bool abort = false;
    
    /// Time for simulation thread logging operations
    Profiler_id prof_event_simulation_thread_log = PROF_REGISTER_EVENT("simulation_thread_log");
    Profiler_id prof_event_add_new_command = PROF_REGISTER_EVENT("add_new_command");
    Profiler_id prof_event_try_new_response = PROF_REGISTER_EVENT("try_new_response");

    /// Non-copyable
    SimulatorRunner(const SimulatorRunner&) = delete;

    /// Non-movable
    SimulatorRunner(const SimulatorRunner&&) = delete;

    /// Non-assignable
    SimulatorRunner& operator=(SimulatorRunner&) = delete;

    /// Non-move-assignable
    SimulatorRunner& operator=(SimulatorRunner&&) = delete;
};
