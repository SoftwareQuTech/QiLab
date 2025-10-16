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
#include "DataTransmission/handlers/spi_master_handler.hpp"
//#include "SimulatorRunner/entanglement_manager.hpp"

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

    StimAdapter* stim_adapter = new StimAdapter(seq, 0); //the seed is problematic
    SimulatorRunner sim(stim_adapter);

    SPIHandler spi_handler;
    if(spi_handler.connect_spi("/dev/spidev0.0", 0, 8, 500000) != 0)
    {
        std::cerr << "Failed to connect to SPI device" << std::endl;
        return -1;
    }
    uint8_t tx[4] = {0x01, 0x02, 0x03, 0x04};
    uint8_t rx[4] = {0};
    spi_handler.transfer(tx, rx, 4);
    std::cout << "SPI received: ";
    for(int i = 0; i < 4; i++)
    {
        std::cout << std::hex << static_cast<int>(rx[i]) << " ";
    }
    std::cout << std::dec << std::endl;

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
    
    PROF_SAVE_DATA("test_trace");
    std::cout << "Executed the simulation" << std::endl;

    return 0;
}