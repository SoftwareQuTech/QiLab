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
#include "CommandHandler/command_handler.hpp"
#include "logger/logger.hpp"
#include "DataTypes/command_data.hpp"

class SimulatorRunner final: public IRunnable<TransportData<CommandBase>>
{
    public:
    SimulatorRunner(ISimulator*);
    ~SimulatorRunner() = default;

    int on_run() override;
    void on_abort() override;

    int16_t register_node(uint8_t num_qubits);
    bool unregister_node(int16_t node_id);
    void set_notify_callback(std::function<void(TransportData<ResponseData>)> cb);

    private:
    ISimulator* simulator;
    IProfiler* event_logger;
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
    std::function<void(TransportData<ResponseData>)> response_notify;

    SimulatorRunner(const SimulatorRunner&) = delete;
    SimulatorRunner(const SimulatorRunner&&) = delete;
    SimulatorRunner& operator=(SimulatorRunner&) = delete;
    SimulatorRunner& operator=(SimulatorRunner&&) = delete;
};
