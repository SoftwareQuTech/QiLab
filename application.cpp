// in a source code file
#include <iostream>
#include "stim.h"
#include <random>
#include "stim/circuit/gate_target.h"
#include "stim/circuit/circuit_instruction.h"

#include <Simulator/adapters/mock_adapter.hpp>
#include <Simulator/adapters/stim_adapter.hpp>
#include <simulator_runner.hpp>
#include <CommandHandler/command_handler.hpp>
#include <CommandHandler/handlers/mock_handler.hpp>
#include "logger/logger.hpp"



int main()
{
    std::random_device d;
    std::seed_seq seq{d(), d(), d(), d(), d(), d(), d(), d()};
    
    ILogger* logger = new LocalLogger();
    MockAdapter* adapter = new MockAdapter();
    StimAdapter* stim_adapter = new StimAdapter(seq, 3); //the seed is problematic
    MockHandler* handler = new MockHandler(logger);
    CommandHandler* command_handler = new CommandHandler(handler, logger);
    SimulatorRunner sim(stim_adapter, command_handler, logger);

    std::cout << "Starting simulation" << std::endl;

    sim.run_simulation();

    logger->save_to_file("test_mock_sim.txt");


    std::cout << "Executed the simulation" << std::endl;

    delete adapter;
    delete handler;
    delete logger;
    delete command_handler;
    
    return 0;
}