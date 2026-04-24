#include <memory>
#include <chrono>
#include <thread>

#include "simulator_runner.hpp"
#include "Simulator/SimulatorManager/simulator_manager.hpp"
#include "utils/thread_utils.hpp"
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

    TransportData<CommandBase> command; // Command from message broker
    TransportData<ResponseData> response;

    LOG_INFO("Starting Simulator Runner with " + std::to_string(simulator_manager->get_registered_nodes()) + " registered nodes.");
    LOG_INFO("Starting Simulator Runner with " + std::to_string(simulator_manager->get_registered_qubits()) + " registered qubits.");

    // Pre-initialize constant fields
    response.destination = DestinationType::CommandRunner;
    response.source = DestinationType::SimRunner;

    while (!abort)
    {
        PROF_START(prof_event_simulation_thread_log);

        if (message_broker->get_command(command, DestinationType::SimRunner))
        {

            ResponseData response_data;
            // LOG_INFO("Received command for node " + std::to_string(command.node_id) + " of type " + std::to_string(command.transport->command_type) + ".");
            auto ret_code = simulator_manager->proccess_command(command.node_id, std::move(command.transport), response_data);

            if (ret_code != 0)
            {
                LOG_ERROR("Error processing command for node " + std::to_string(command.node_id) + ". Error code: " + std::to_string(ret_code));
            }
            else
            {
                response.transport = std::make_unique<ResponseData>(std::move(response_data));

                if (!message_broker->route_response(std::move(response)))
                {
                    // error
                    LOG_ERROR("Failed to route response for node " + std::to_string(command.node_id) + ".");
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::microseconds(5));
        PROF_STOP(prof_event_simulation_thread_log);
        // CPU_PAUSE();
        // std::this_thread::yield();
    }
    PROF_THREAD_FLUSH();
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
