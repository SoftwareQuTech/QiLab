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
#include <SimulatorRunners/simulator_runner.hpp>
#include "DataTransmission/TransmissionRunner/transmission_runner.hpp"
#include "DataTransmission/TransmissionProcessor/transmission_processor.hpp"
#include "DataTransmission/handlers/mock_handler.hpp"
#include "logger/profiler_api.hpp"
#include "logger/logger_api.hpp"
#include "DataTransmission/handlers/spi_master_handler.hpp"
#include "SimulatorRunners/entanglement_runner.hpp"
#include "MessageBroker/message_broker.hpp"

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

    std::shared_ptr<MessageBroker> broker = std::make_shared<MessageBroker>();
    
    std::unique_ptr<StimAdapter> stim_adapter =  std::make_unique<StimAdapter>(seq, 0); //the seed is problematic
    SimulatorRunner sim(std::move(stim_adapter), broker);
    int16_t node_id_1 = sim.register_node(10);
    int16_t node_id_2 = sim.register_node(10);
    //int16_t node_id_3 = sim.register_node(10);
    //int16_t node_id_4 = sim.register_node(10);


    std::unique_ptr<ITransmissionHandler> transportB = std::make_unique<MockHandler>();
    std::unique_ptr<ITransmissionHandler> transportA = std::make_unique<SPIHandler>();
    std::unique_ptr<TransmissionProcessor> command_handlerA = std::make_unique<TransmissionProcessor>(std::move(transportA));
    DataTransmissionRunner nodeA({node_id_1, node_id_2}, std::move(command_handlerA), broker);

    EntanglementRunner ent_runner(broker);

    LOG_INFO("Starting simulation");
    
    // Pre-define core assignments
    constexpr int SIMULATOR_CORE = 2;
    constexpr int ENTANGLEMENT_CORE = 2;
    constexpr int TRANSMISSION_CORE = 3;

    // Start simulator thread with affinity check
    LOG_INFO("Starting simulator thread on core: " + std::to_string(SIMULATOR_CORE));
    std::jthread simulator_thread([&sim, SIMULATOR_CORE]() {
        // Set name for easier identification
        pthread_setname_np(pthread_self(), "simulator");
        return sim.on_run(SIMULATOR_CORE);
    });

    // Small delay to ensure first thread is settled
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Start entanglement thread with affinity check
    LOG_INFO("Starting entanglement thread on core: " + std::to_string(ENTANGLEMENT_CORE));
    std::jthread entanglement_thread([&ent_runner, ENTANGLEMENT_CORE]() {
        // Set name for easier identification
        pthread_setname_np(pthread_self(), "entanglement");
        return ent_runner.on_run(ENTANGLEMENT_CORE);
    });

    // Small delay to ensure second thread is settled
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Start transmission thread with affinity check
    LOG_INFO("Starting transmission thread on core: " + std::to_string(TRANSMISSION_CORE));
    std::jthread thread_node_a([&nodeA, TRANSMISSION_CORE]() {
        // Set name for easier identification
        pthread_setname_np(pthread_self(), "transmit");
        return nodeA.on_run(TRANSMISSION_CORE);
    });


    while(!abort_sim)
    {
        //do nothing and wait for abort signal
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        //abort_sim = true;
    }

    sim.on_abort();
    simulator_thread.join();
    
    ent_runner.on_abort();
    entanglement_thread.join();

    nodeA.on_abort();    
    thread_node_a.join();
/*
    std::shared_ptr<ITransmissionHandler> transportA = std::make_shared<MockHandler>();
    TransmissionProcessor* command_handlerA = new TransmissionProcessor(transportA);
    DataTransmissionRunner* nodeA = new DataTransmissionRunner({0}, command_handlerA, nullptr);


    int16_t node_id = sim.register_node(10);


    std::cout << "Starting simulation" << std::endl;

    std::jthread simulator_thread(&SimulatorRunner::on_run, &sim);
    //std::jthread ent_manager_thread(&EntanglementManager::on_run, &ent_manager);

    std::unique_ptr<std::jthread> thread_node_a;


    if(nodeA != nullptr)
    {
        thread_node_a = std::make_unique<std::jthread>([nodeA]{ nodeA->on_run(); });
    }

    while(!abort_sim)
    {
        //do nothing and wait for abort signal
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        abort_sim = true;
    }
    std::cout << "Timer expired" << std::endl;

    nodeA->on_abort();

    std::cout << "Node Stopped" << std::endl;

    sim.on_abort();
        std::cout << "Sim stopped" << std::endl;

    simulator_thread.join();
        std::cout << "Join sim thread" << std::endl;

    thread_node_a->join();

    std::cout << "Join node thread" << std::endl;


    //logger->save_to_file("simple_prg_10000r_3n");

    delete stim_adapter;
    delete command_handlerA;
    */
    PROF_SAVE_DATA("test_trace");
    LOG_INFO("Executed the simulation");

    return 0;
}