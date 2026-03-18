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

#include "DataTypes/commands.hpp"
#include "DataTypes/responses.hpp"
#include "Simulator/sim_interface.hpp"
#include "qubit_model.hpp"

class SimulatorManager
{
public:
    SimulatorManager(std::unique_ptr<ISimulator> simulator);
    ~SimulatorManager();

    ResponseData proccess_command(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id);
    int add_qubits(int num_qubits, float t2_value);
private:

    using SimCommandFn = ResponseData (SimulatorManager::*)(std::unique_ptr<CommandBase> command,
                                                            int16_t qubit_offset,
                                                            uint8_t node_id);

    static constexpr int MAX_COMMANDS_AVAILABLE = 64;
    static constexpr int MAX_QUBITS_AVAILABLE = 256;

    std::array<SimCommandFn, MAX_COMMANDS_AVAILABLE> sim_command_table = {};
    std::array<QubitModel, MAX_QUBITS_AVAILABLE> simulation_qubits;
    /// The underlying quantum simulator
    std::unique_ptr<ISimulator> simulator_inst;
    int registered_qubits = 0;

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

};
