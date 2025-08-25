/*
** Created on: 25.08.2025
** Created by: mrkolev
** Description: Contains simulator runner code. It handles the commands
**              and gives them to the simulator
*/

#pragma once

#include "Simulator/sim_interface.hpp"
#include "CommandHandler/command_handler.hpp"

class SimulatorRunner
{
    public:
    SimulatorRunner(ISimulator*, CommandHandler*);
    ~SimulatorRunner() = default;

    void run_simulation();

    private:
    ISimulator* simulator;
    CommandHandler* command_handler;
    ResponseData execute_command(CommandBase*);

    SimulatorRunner(const SimulatorRunner&) = delete;
    SimulatorRunner(const SimulatorRunner&&) = delete;
    SimulatorRunner& operator=(SimulatorRunner&) = delete;
    SimulatorRunner& operator=(SimulatorRunner&&) = delete;
};
