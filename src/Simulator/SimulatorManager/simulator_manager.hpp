/*
 * simulator_manager.hpp
 *
 *  Created on: Apr 27, 2024
 *      Author: Milen Kolev
 *      Description: The fiel contains declaration for the manager class. 
 *                  It contains logic for calling the correct sim functions and 
 *                  managing the simulation state.
 */

#pragma once
#include "memory"
#include "array"

#include "DataTypes/commands.hpp"
#include "DataTypes/responses.hpp"
#include "Simulator/sim_interface.hpp"
#include "Simulator/qubit_noise_interface.hpp"
#include "DataTypes/configuration_options.hpp"
#include "logger/profiler_api.hpp"

class SimulatorManager
{
public:
    SimulatorManager(std::unique_ptr<ISimulator> simulator);
    ~SimulatorManager();

        /**
     * @brief Register a new quantum node with the specified number of qubits.
     *
     * Nodes are registered sequentially, each receiving a unique ID based on
     * registration order. Qubits are allocated from a shared pool with per-node
     * offsets to support independent addressing. 
     * One node could have qubits with different models
     *
     * @param node_config Configuration data for the node to register
     * 
     * @return return code 0 success
     * @return -1 if registration fails (max nodes reached or insufficient qubits)
     *
     * @note Node IDs are assigned sequentially starting from 0
     * @note Qubits are allocated contiguously in global address space
     */
    int16_t register_node(const NodeConfig &node_config);

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

    int proccess_command(uint16_t node_id, std::unique_ptr<CommandBase> command, ResponseData& response);

    const uint16_t get_registered_nodes() const { return registered_nodes; }
    const uint16_t get_registered_qubits() const { return registered_qubits; }
private:

    using SimCommandFn = ResponseData (SimulatorManager::*)(std::unique_ptr<CommandBase> command,
                                                            int16_t qubit_offset,
                                                            uint8_t node_id);

    /// Maximum number of handler functions that can be registered in the command dispatch table                                                          
    static constexpr int MAX_COMMANDS_AVAILABLE = 64;
    /// Maximum number of qubits that can be allocated across all nodes
    static constexpr int MAX_QUBITS_AVAILABLE = 1024;
    /// Maximum number of nodes that can be registered simultaneously
    static constexpr int MAX_NODES_AVAILABLE = 4;

    /// Registry of nodes. Index is node_id; value is (qubit_offset, qubit_count) pair
    /// Qubit offset is the starting address for this node's qubits in global space
    std::array<std::pair<uint16_t, uint16_t>, MAX_NODES_AVAILABLE> nodes;
    uint16_t registered_nodes = 0; // Number of currently registered nodes

    /// Total number of qubits already allocated across all nodes
    uint16_t registered_qubits = 0;
    std::array<std::unique_ptr<QubitNoiseInterface>, MAX_QUBITS_AVAILABLE> simulation_qubits;

    /// The underlying quantum simulator
    std::unique_ptr<ISimulator> simulator_inst;

    std::array<SimCommandFn, MAX_COMMANDS_AVAILABLE> sim_command_table = {};

    bool is_local_qubit_in_range(uint16_t node_id, uint16_t local_qubit) const;
    bool are_local_qubits_in_range(uint16_t node_id, uint16_t local_qubit1, uint16_t local_qubit2) const;
    
    ResponseData nop_command_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id);
    ResponseData single_qubit_gate_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id);
    ResponseData two_qubit_gate_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id);
    ResponseData qubit_measure_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id);
    ResponseData qubit_init_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id);
    ResponseData entanglement_attempt_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id);
    ResponseData entanglement_measure_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id);
    ResponseData entanglement_swap_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id);
    ResponseData pre_measure_and_entangle_measure_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id);
    ResponseData pre_measurement_gates_selection_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id);
    ResponseData qubit_move_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id);
    ResponseData qubit_swap_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id);


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


};
