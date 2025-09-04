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
#if defined(__linux) && defined(__GLIBCXX__) && __GLIBCXX__ >= 20200128
    // Workaround for https://gcc.gnu.org/bugzilla/show_bug.cgi?id=94087
    // See https://github.com/quantumlib/Stim/issues/26
    std::random_device d("/dev/urandom");
#else
    std::random_device d;
#endif
    std::seed_seq seq{d(), d(), d(), d(), d(), d(), d(), d()};

    IProfiler* logger = new LocalProfiler();
    MockAdapter* adapter = new MockAdapter();
    StimAdapter* stim_adapter = new StimAdapter(seq, 3); //the seed is problematic
    MockHandler* handler = new MockHandler(logger);
    CommandHandler* command_handler = new CommandHandler(handler, logger);
    SimulatorRunner sim(stim_adapter, command_handler, logger);

    std::cout << "Starting simulation" << std::endl;

    sim.run_simulation();

    logger->save_to_file("test_mock_sim");


    std::cout << "Executed the simulation" << std::endl;

    delete adapter;
    delete handler;
    delete logger;
    delete command_handler;
    
    return 0;
}