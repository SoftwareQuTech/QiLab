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

struct NodeInfo; // Forward declaration of NodeInfo struct
struct Qubit; // Forward declaration of Qubit struct

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
     * @param sim Unique pointer to the ISimulator implementation to use for operations
     * @param broker Shared pointer to the MessageBroker for command/response routing
     */
    SimulatorRunner(std::unique_ptr<ISimulator> sim, std::shared_ptr<MessageBroker> broker);

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

    /**
     * @brief Register a new quantum node with the specified number of qubits.
     *
     * Nodes are registered sequentially, each receiving a unique ID based on
     * registration order. Qubits are allocated from a shared pool with per-node
     * offsets to support independent addressing.
     *
     * @param num_qubits Number of qubits to allocate for this node
     * 
     * @return Node ID (0-based index) on successful registration
     * @return -1 if registration fails (max nodes reached or insufficient qubits)
     *
     * @note Node IDs are assigned sequentially starting from 0
     * @note Qubits are allocated contiguously in global address space
     */
    int16_t register_node(uint8_t num_qubits);

    /**
     * @brief Unregister a quantum node and free its qubits.
     *
     * @param node_id The ID of the node to unregister
     * 
     * @return true if the node was successfully unregistered
     * @return false if the node could not be unregistered (not implemented)
     *
     * @note Currently not implemented; returns false
     */
    bool unregister_node(int16_t node_id);

    private:
    /// Reference to the message broker for command/response routing
    std::shared_ptr<MessageBroker> message_broker;

    /// The underlying quantum simulator
    std::unique_ptr<ISimulator> simulator;

    /// Maximum number of nodes that can be registered simultaneously
    const uint16_t max_nodes_available = 4;

    /// Maximum total qubits available for allocation
    /// Note: Stim (the underlying simulator) expands tableau automatically;
    /// this is just a practical memory constraint
    const uint16_t max_qubits_available = 200;

    /// Registry of nodes. Index is node_id; value is (qubit_offset, qubit_count) pair
    /// Qubit offset is the starting address for this node's qubits in global space
    std::vector<std::pair<uint16_t, uint16_t>> nodes;

    /// Total number of qubits already allocated across all nodes
    uint16_t registered_qubits = 0;

    /// Flag to signal thread termination
    bool abort = false;

    /**
     * @brief Executes a single command on the simulator.
     *
     * Dispatches the command to the underlying simulator with the appropriate
     * qubit offset for the target node.
     *
     * @param command The command to execute (moved)
     * @param qubit_offset The base qubit address for the target node
     * @param node_id The ID of the node executing this command (used for entanglement initiator determination)
     * 
     * @return ResponseData containing the result of command execution
     */
    ResponseData execute_command(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id);

    /// Profiling event IDs for performance measurement

    /// Time to execute any command type
    Profiler_id prof_event_execute_command = PROF_REGISTER_EVENT("execute_command");

    /// Time for no-op (placeholder) commands
    Profiler_id prof_event_command_nop = PROF_REGISTER_EVENT("command_nop");

    /// Time to initialize/reset qubits
    Profiler_id prof_event_qubit_init = PROF_REGISTER_EVENT("qubit_init");

    /// Time to measure qubits
    Profiler_id prof_event_qubit_measurement = PROF_REGISTER_EVENT("qubit_measurement");

    /// Time to apply single-qubit gates
    Profiler_id prof_event_single_qubit_gate = PROF_REGISTER_EVENT("single_qubit_gate");

    /// Time to apply two-qubit gates
    Profiler_id prof_event_two_qubit_gate = PROF_REGISTER_EVENT("two_qubit_gate");

    /// Time to swap qubits
    Profiler_id prof_event_qubit_swap = PROF_REGISTER_EVENT("qubit_swap");

    /// Time to attempt entanglement operation
    Profiler_id prof_event_entanglement_attempt = PROF_REGISTER_EVENT("entanglement_attempt");

    /// Time to perform entanglement swap
    Profiler_id prof_event_entanglement_swap = PROF_REGISTER_EVENT("entanglement_swap");

    /// Time to measure entangled qubits
    Profiler_id prof_event_entanglement_measure = PROF_REGISTER_EVENT("entanglement_measure");

    /// Time for pre-measurement basis selection
    Profiler_id prof_event_pre_measure_select = PROF_REGISTER_EVENT("pre_measure_select");

    /// Time to move qubits between nodes
    Profiler_id prof_event_qubit_move = PROF_REGISTER_EVENT("qubit_move");

    /// Time for simulation thread logging operations
    Profiler_id prof_event_simulation_thread_log = PROF_REGISTER_EVENT("simulation_thread_log");

    /// Non-copyable
    SimulatorRunner(const SimulatorRunner&) = delete;

    /// Non-movable
    SimulatorRunner(const SimulatorRunner&&) = delete;

    /// Non-assignable
    SimulatorRunner& operator=(SimulatorRunner&) = delete;

    /// Non-move-assignable
    SimulatorRunner& operator=(SimulatorRunner&&) = delete;
};
