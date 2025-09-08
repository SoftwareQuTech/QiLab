/*
** Created on: 25.08.2025
** Created by: mrkolev
** Description: Contains simulator runner code. It handles the commands
**              and gives them to the simulator
*/

#pragma once
#include <map>

#include "runnable.hpp"
#include "Simulator/sim_interface.hpp"
#include "CommandHandler/command_handler.hpp"
#include "logger/logger.hpp"
#include "DataTypes/command_data.hpp"

class SimulatorRunner final: IRunnable<CommandData_t>
{
    public:
    SimulatorRunner(ISimulator*);
    SimulatorRunner(ISimulator*, IProfiler*);
    ~SimulatorRunner() = default;

    void on_run() override;
    void on_abort() override;

    int16_t register_node(uint8_t num_qubits);
    bool unregister_node(int16_t node_id);

    private:
    ISimulator* simulator;
    IProfiler* event_logger;
    std::map<int16_t, uint8_t> nodes;
    uint16_t registered_qubits = 0;

    ResponseData execute_command(CommandBase*);

    SimulatorRunner(const SimulatorRunner&) = delete;
    SimulatorRunner(const SimulatorRunner&&) = delete;
    SimulatorRunner& operator=(SimulatorRunner&) = delete;
    SimulatorRunner& operator=(SimulatorRunner&&) = delete;
};
