/*
** Created on: 25.08.2025
** Created by: mrkolev
** Description: Contains simulator runner code. It handles the commands
**              and gives them to the simulator
*/

#pragma once
#include <vector>

#include "runnable.hpp"
#include "Simulator/sim_interface.hpp"
#include "logger/profiler_api.hpp"
#include "DataTypes/command_data.hpp"

class SimulatorRunner
{
    public:
    SimulatorRunner(ISimulator*);
    ~SimulatorRunner() = default;

    int on_run();
    void on_abort();

    int16_t register_node(uint8_t num_qubits);
    bool unregister_node(int16_t node_id);

    private:
    ISimulator* simulator;
    //constrain on how much nodes to register
    const uint16_t max_nodes_available = 20;
    //NOTE: the tableuau stim expands automatically with requested address.
    //no need for preallocation, just a constrain for memory
    const uint16_t max_qubits_available = 200;

    //the register of nodes. The IDs are the vector indeces,
    //the pair represents initial qubit address(offset) and number of qubits 
    //registered for the node
    std::vector<std::pair<uint16_t,uint16_t>> nodes;
    uint16_t registered_qubits = 0;

    bool abort = false;

    ResponseData execute_command(std::unique_ptr<CommandBase> command, int16_t qubit_offset);

    //profiling event ids
    Profiler_id prof_event_command_nop = PROF_REGISTER_EVENT("command_nop");
    Profiler_id prof_event_qubit_init = PROF_REGISTER_EVENT("qubit_init");
    Profiler_id prof_event_qubit_measurement = PROF_REGISTER_EVENT("qubit_measurement");
    Profiler_id prof_event_single_qubit_gate = PROF_REGISTER_EVENT("single_qubit_gate");
    Profiler_id prof_event_two_qubit_gate = PROF_REGISTER_EVENT("two_qubit_gate");
    Profiler_id prof_event_qubit_swap = PROF_REGISTER_EVENT("qubit_swap");
    Profiler_id prof_event_entanglement_attempt = PROF_REGISTER_EVENT("entanglement_attempt");
    Profiler_id prof_event_entanglement_swap = PROF_REGISTER_EVENT("entanglement_swap");
    Profiler_id prof_event_entanglement_measure = PROF_REGISTER_EVENT("entanglement_measure");
    Profiler_id prof_event_pre_measure_select = PROF_REGISTER_EVENT("pre_measure_select");
    Profiler_id prof_event_qubit_move = PROF_REGISTER_EVENT("qubit_move");
    Profiler_id prof_event_simulation_thread_log = PROF_REGISTER_EVENT("simulation_thread_log");

    SimulatorRunner(const SimulatorRunner&) = delete;
    SimulatorRunner(const SimulatorRunner&&) = delete;
    SimulatorRunner& operator=(SimulatorRunner&) = delete;
    SimulatorRunner& operator=(SimulatorRunner&&) = delete;
};
