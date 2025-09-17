// in a source code file
#include <iostream>
#include <random>
#include <thread>
#include <signal.h>

#include "stim.h"
#include "stim/circuit/gate_target.h"
#include "stim/circuit/circuit_instruction.h"

#include <Simulator/adapters/mock_adapter.hpp>
#include <Simulator/adapters/stim_adapter.hpp>
#include <SimulatorRunner/simulator_runner.hpp>
#include <CommandHandler/command_handler.hpp>
#include "CommandNode/command_node.hpp"
#include <CommandHandler/handlers/mock_handler.hpp>
#include "logger/logger.hpp"

bool abort_sim = false;


void signal_callback_handler(int signum) {
   std::cout << "Caught signal " << signum << std::endl;
   // Terminate program
   abort_sim = true;
}

int main()
{
    signal(SIGINT, signal_callback_handler);
#if defined(__linux) && defined(__GLIBCXX__) && __GLIBCXX__ >= 20200128
    // Workaround for https://gcc.gnu.org/bugzilla/show_bug.cgi?id=94087
    // See https://github.com/quantumlib/Stim/issues/26
    std::random_device d("/dev/urandom");
#else
    std::random_device d;
#endif
    std::seed_seq seq{d(), d(), d(), d(), d(), d(), d(), d()};
    std::vector<CommandNode> command_nodes;

    IProfiler* logger = new LocalProfiler();
    StimAdapter* stim_adapter = new StimAdapter(seq, 0); //the seed is problematic
    SimulatorRunner sim(stim_adapter, logger);

    MockHandler* transportA = new MockHandler(logger);
    CommandHandler* command_handlerA = new CommandHandler(transportA, logger);
    CommandNode* nodeA;

    MockHandler* transportB = new MockHandler(logger);
    CommandHandler* command_handlerB = new CommandHandler(transportB, logger);
    CommandNode* nodeB;

    MockHandler* transportC = new MockHandler(logger);
    CommandHandler* command_handlerC = new CommandHandler(transportC, logger);
    CommandNode* nodeC;

    MockHandler* transportD = new MockHandler(logger);
    CommandHandler* command_handlerD = new CommandHandler(transportD, logger);
    CommandNode* nodeD;

    int16_t node_id = sim.register_node(10);
    if(node_id != -1)
    {
        nodeA = new CommandNode(node_id, command_handlerA, std::bind(&SimulatorRunner::notify, &sim, std::placeholders::_1), logger);
    }

    node_id = sim.register_node(10);
    if(node_id != -1)
    {
        nodeB = new CommandNode(node_id, command_handlerB, std::bind(&SimulatorRunner::notify, &sim, std::placeholders::_1), logger);
    }

    node_id = sim.register_node(10);
    if(node_id != -1)
    {
        nodeC = new CommandNode(node_id, command_handlerC, std::bind(&SimulatorRunner::notify, &sim, std::placeholders::_1), logger);
    }

    node_id = sim.register_node(10);
    if(node_id != -1)
    {
        nodeD = new CommandNode(node_id, command_handlerD, std::bind(&SimulatorRunner::notify, &sim, std::placeholders::_1), logger);
    }
    std::cout << "Starting simulation" << std::endl;

    std::thread simulator_thread(&SimulatorRunner::on_run, &sim);

    std::unique_ptr<std::jthread> thread_node_a;
    std::unique_ptr<std::jthread> thread_node_b;
    std::unique_ptr<std::jthread> thread_node_c;
    std::unique_ptr<std::jthread> thread_node_d;

    if(nodeA != nullptr)
    {
        thread_node_a = std::make_unique<std::jthread>([nodeA]{ nodeA->run_node(); });
    }

    if(nodeB != nullptr)
    {
        thread_node_b = std::make_unique<std::jthread>([nodeB]{ nodeB->run_node(); });
    }

    if(nodeC != nullptr)
    {
        thread_node_c = std::make_unique<std::jthread>([nodeC]{ nodeC->run_node(); });
    }

    if(nodeD != nullptr)
    {
        thread_node_d = std::make_unique<std::jthread>([nodeD]{ nodeD->run_node(); });
    }
    while(!abort_sim)
    {
        //do nothing and wait for abort signal
        std::this_thread::sleep_for(std::chrono::seconds(240));
        abort_sim = true;
    }
    std::cout << "Timer expired" << std::endl;

    nodeA->stop_node();
    nodeB->stop_node();
    nodeD->stop_node();
    nodeC->stop_node();

    std::cout << "Node Stopped" << std::endl;

    sim.on_abort();
        std::cout << "Sim stopped" << std::endl;

    simulator_thread.join();
        std::cout << "Join sim thread" << std::endl;

    thread_node_a->join();
    thread_node_b->join();
    thread_node_c->join();
    thread_node_d->join();

        std::cout << "Join node thread" << std::endl;


    logger->save_to_file("simple_prg_10000r_3n");

    std::cout << "Executed the simulation" << std::endl;

    delete stim_adapter;
    delete transportA;
    delete command_handlerA;
    delete transportB;
    delete command_handlerB;
    delete transportC;
    delete command_handlerC;
    delete transportD;
    delete command_handlerD;
    delete logger;

    return 0;
}