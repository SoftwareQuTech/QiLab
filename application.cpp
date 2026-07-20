// in a source code file
#include <iostream>
#include <random>
#include <string_view>
#include <thread>
#include <signal.h>


//Simulator implementations
#include <Simulator/SimulatorAdapters/mock_adapter.hpp>
#include <Simulator/SimulatorAdapters/stim_adapter.hpp>
#include <Simulator/SimulatorAdapters/stim_nv_adapter.hpp>

//Runners
#include "Simulator/SimulatorRunners/simulator_runner.hpp"
#include "DataTransmission/TransmissionRunner/transmission_runner.hpp"
#include "Simulator/SimulatorRunners/entanglement_runner.hpp"

//Intermediaries
#include "Simulator/SimulatorManager/simulator_manager.hpp"

//Data transmission
#include "DataTransmission/TransmissionProcessor/transmission_processor.hpp"
#include "DataTransmission/handlers/mock_handler.hpp"
#include "DataTransmission/handlers/spi_master_handler.hpp"
#include "MessageBroker/message_broker.hpp"

//Loggers and profilers
#include "logger/profiler_api.hpp"
#include "logger/logger_api.hpp"
#include "logger/data_acquisition_api.hpp"

//Config
#include "qilab/version.hpp"
#include "ConfigurationEngine/config_loader.hpp"

//#define RUN_MOCKED_TRANSMISSION

bool abort_sim = false;

void print_startup_banner(const std::string& version) {
    std::cout << "\n"
              << "==== QILAB :: Quantum Interface Lab ====\n"
              << "Product of TU Delft | v" << version << "\n"
              << "========================================\n"
              << std::endl;
}


void signal_callback_handler(int signum) {
   // Terminate program
   abort_sim = true;
}

int main()
{
    const std::string version = std::to_string(QILab::VERSION_MAJOR) + "."
                              + std::to_string(QILab::VERSION_MINOR) + "."
                              + std::to_string(QILab::VERSION_PATCH)
                              + " (" + QILab::VERSION_GIT_COMMIT + ")";

    print_startup_banner(version);

    signal(SIGINT, signal_callback_handler);
    signal(SIGTERM, signal_callback_handler);
    //get the log level from environment variable and set it in the logger
    const char* log_level_env = std::getenv("QILAB_LOG_LEVEL");
    if (log_level_env) {
        std::string log_level_str(log_level_env);
        LOG_LEVEL(log_level_str);
    } else {
        LOG_LEVEL("DEBUG"); // default log level
    }
    // Structured research data acquisition is opt-in via QILAB_DAQ=1. Output
    // goes to ~/.qilab/logs/data_acquisition.jsonl, next to the profiler data.
    const char* daq_env = std::getenv("QILAB_DAQ");
    const bool daq_enabled = daq_env && std::string_view(daq_env) == "1";
    DAQ_CONFIGURE(daq_enabled, "");

#if defined(__linux) && defined(__GLIBCXX__) && __GLIBCXX__ >= 20200128
    // Workaround for https://gcc.gnu.org/bugzilla/show_bug.cgi?id=94087
    // See https://github.com/quantumlib/Stim/issues/26
    std::random_device d("/dev/urandom");
#else
    std::random_device d;
#endif
    std::seed_seq seq{d(), d(), d(), d(), d(), d(), d(), d()};

    std::shared_ptr<MessageBroker> broker = std::make_shared<MessageBroker>();
    
    // Load configuration from file
    const std::string config_path = get_default_config_path();
    LOG_INFO("Using configuration file: " + config_path);
    auto config = load_config(config_path);
    if (!config) {
        LOG_ERROR("Failed to load configuration, exiting");
        return 1;
    }
    
    if (config->nodes.empty()) {
        LOG_ERROR("No nodes configured, exiting");
        return 1;
    }
    
    LOG_INFO("Configuration loaded with " + std::to_string(config->nodes.size()) + " nodes");
    
    std::unique_ptr<StimAdapterNVCenters> stim_adapter =  std::make_unique<StimAdapterNVCenters>(seq, 0); //the seed is problematic
    std::unique_ptr<SimulatorManager> sim_manager = std::make_unique<SimulatorManager>(std::move(stim_adapter), config->system.qubit_noise_enabled);
    
    
    // Register nodes dynamically from configuration
    std::vector<int16_t> node_ids;
    for (size_t i = 0; i < config->nodes.size(); ++i) {
        int16_t node_id = sim_manager->register_node(config->nodes[i]);
        node_ids.push_back(node_id);
        LOG_INFO("Registered node " + std::to_string(i) + " with ID " + 
                 std::to_string(node_id) + " (" + std::to_string(sim_manager->get_registered_qubits()) + " qubits)");
    }

    SimulatorRunner sim(std::move(sim_manager), broker);

#ifndef RUN_MOCKED_TRANSMISSION
    LOG_INFO("Using SPI transmission handler");
    std::unique_ptr<ITransmissionHandler> transportA = std::make_unique<SPIHandler>();
    std::unique_ptr<TransmissionProcessor> command_handlerA = std::make_unique<TransmissionProcessor>(std::move(transportA));
    DataTransmissionRunner transmission_node(node_ids, std::move(command_handlerA), broker, config->system.transmission_cycle_time_us);
#else
    LOG_INFO("Using mocked transmission handler");
    std::unique_ptr<ITransmissionHandler> transportB = std::make_unique<MockHandler>();
    std::unique_ptr<TransmissionProcessor> command_handlerB = std::make_unique<TransmissionProcessor>(std::move(transportB));
    DataTransmissionRunner transmission_node(node_ids, std::move(command_handlerB), broker, config->system.transmission_cycle_time_us);
#endif

    EntanglementRunner ent_runner(broker);

    LOG_INFO("Starting simulation");

    LOG_WARNING("This build uses reset of the qubits in the entanglement attempt. This is needed in order to run the qia-test apps correctly.");
    LOG_WARNING("This will be removed in the future when we have a better way to ensure state of the qubits for testing.");

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
    std::jthread thread_node_a([&transmission_node, TRANSMISSION_CORE]() {
        // Set name for easier identification
        pthread_setname_np(pthread_self(), "transmit");
        return transmission_node.on_run(TRANSMISSION_CORE);
    });

    PROF_SAVE_DATA("test_trace");

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

    transmission_node.on_abort();    
    thread_node_a.join();

    PROF_SAVE_DATA("test_trace");
    LOG_INFO("Executed the simulation");

    return 0;
}