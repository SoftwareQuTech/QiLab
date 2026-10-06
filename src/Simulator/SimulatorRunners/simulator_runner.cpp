// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <memory>
#include <chrono>
#include <thread>

#include "simulator_runner.hpp"
#include "Simulator/SimulatorManager/simulator_manager.hpp"
#include "utils/thread_utils.hpp"
#include "utils/periodic_pacer.hpp"
#include "logger/logger_api.hpp"

SimulatorRunner::SimulatorRunner(std::unique_ptr<SimulatorManager> sim, std::shared_ptr<MessageBroker> broker) : simulator_manager(std::move(sim)), message_broker(broker)
{
    PROF_THREAD_INIT();
    // PROF_EID("command_nop");
}

int SimulatorRunner::on_run(int core_id)
{
    // Set affinity at the very start
    if (core_id >= 0)
    {
        // Try to pin thread and verify success
        if (!thread_utils::pin_current_thread_to_core(core_id))
        {
            LOG_ERROR("WARNING: Failed to properly pin SimulatorRunner to core " + std::to_string(core_id) + ".");
        }

        // Double check after a small delay
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        int current_core = sched_getcpu();

        if (current_core != core_id)
        {
            LOG_WARNING("SimulatorRunner still not on correct core. "
                        "Expected " +
                        std::to_string(core_id) + " but on " + std::to_string(current_core) + ".");
        }
    }

    ProfGuard pg{};

    // Poll interval for the command/response pump. This runner is pinned and running
    // at SCHED_FIFO(80), so it must block every cycle or it starves the per-CPU kernel
    // threads on its core exactly as the transmission runner's old spin did — less
    // likely here only because this loop has always had a sleep in it.
    constexpr auto kPollInterval = std::chrono::microseconds(5);

    TransportData<CommandBase> command; // Command from message broker
    TransportData<ResponseData> response;

    LOG_INFO("Starting Simulator Runner with " + std::to_string(simulator_manager->get_registered_nodes()) + " registered nodes.");
    LOG_INFO("Starting Simulator Runner with " + std::to_string(simulator_manager->get_registered_qubits()) + " registered qubits.");

    // Pre-initialize constant fields
    response.destination = DestinationType::CommandRunner;
    response.source = DestinationType::SimRunner;

    thread_utils::PeriodicPacer pacer{kPollInterval};

    while (!abort)
    {
        ResponseData response_data;
        PROF_START(prof_event_simulation_thread_log);

        if (message_broker->get_command(command, DestinationType::SimRunner))
        {
            PROF_START(prof_event_add_new_command);

            // ResponseData response_data;
            // LOG_INFO("Received command for node " + std::to_string(command.node_id) + " of type " + std::to_string(command.transport->command_type) + ".");
            auto ret_code = simulator_manager->add_new_command_to_process(command.node_id, std::move(command.transport), response_data);

            if (ret_code != 0)
            {
                LOG_ERROR("Error processing command for node " + std::to_string(command.node_id) + ". Error code: " + std::to_string(ret_code));
            }
            PROF_STOP(prof_event_add_new_command);
        }
        PROF_START(prof_event_try_new_response);
        uint16_t node_id = 0; // Placeholder, actual node_id should be determined based on the command being processed
        if (simulator_manager->try_get_ready_response(response_data, node_id) == 0)
        {
            response.transport = std::make_unique<ResponseData>(std::move(response_data));
            response.node_id = node_id; 
            if (!message_broker->route_response(std::move(response)))
            {
                // error
                LOG_ERROR("Failed to route response for node " + std::to_string(node_id) + ".");
            }
        }
        else
        {
            // No response ready, can perform other tasks or simply continue
            // LOG_INFO("No ready response from simulator manager.");
        }
        PROF_STOP(prof_event_try_new_response);
        PROF_STOP(prof_event_simulation_thread_log);

        // Absolute-deadline pace: drift-free, and guarantees the core is released even
        // when a command takes longer than the poll interval to process.
        pacer.wait_for_next_cycle();
    }
    PROF_THREAD_FLUSH();
    LOG_INFO("Simulator Runner pacing: " + thread_utils::pacing_summary(pacer) + ".");
    LOG_INFO("Ending Simulator Runner.");
    // optional: drain/notify when aborting
    return 1;
}

void SimulatorRunner::on_abort()
{
    // maybe run cleanup
    LOG_INFO("SimulatorRunner abort requested.");
    abort = true;
}
