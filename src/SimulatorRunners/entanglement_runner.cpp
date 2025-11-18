#include <thread>

#include "entanglement_runner.hpp"
#include "DataTypes/commands.hpp"
#include "utils/thread_utils.hpp"
#include "logger/logger_api.hpp"

EntanglementRunner::EntanglementRunner(std::shared_ptr<MessageBroker> mb)
    : message_broker(mb), entanglement_manager(std::make_unique<EntanglementManager>(
                              [this](int node_id, Outcome_Status_t status)
                              {
                                  send_response_async(node_id, status);
                              },
                              [this](int node_id, std::unique_ptr<CommandBase> cmd)
                              {
                                  send_command_async(node_id, std::move(cmd));
                              })) {}

int EntanglementRunner::on_run(int core_id)
{
    // Set affinity at the very start
    if (core_id >= 0)
    {
        // Try to pin thread and verify success
        if (!thread_utils::pin_current_thread_to_core(core_id))
        {
            LOG_ERROR("Failed to properly pin SimulatorRunner to core " + std::to_string(core_id) + ".");
        }

        // Double check after a small delay
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        int current_core = sched_getcpu();
        if (current_core != core_id)
        {
            LOG_WARNING("SimulatorRunner still not on correct core. Expected " + std::to_string(core_id) + " but on " + std::to_string(current_core) + ".");
        }
    }
    TransportData<CommandBase> command; // Command from message broker

    while (!abort)
    {
        if(message_broker->get_command(command, DestinationType::EntanglementRunner))
        {
            //process command
            auto cmd = std::move(command.transport);
            uint16_t node_id = command.node_id;
            uint16_t neighbour_node = 0; // TODO: extract from cmd

            //extract neighbour node from command - placeholder here
            if(cmd->command_type == Command_t::ENTANGLEMENT_ATTEMPT)
            {
                neighbour_node = static_cast<EntanglementAttempt*>(cmd.get())->nghbr;
            }
            else if(cmd->command_type == Command_t::ENTANGLEMENT_MEASURE)
            {
                neighbour_node = static_cast<EntangleMeasure*>(cmd.get())->nghbr;
            }
            else
            {
                LOG_WARNING("Received non-entanglement command in EntanglementRunner. Ignoring.");
                continue; // not an entanglement command
            }

            if(!entanglement_manager->add_new_request(std::move(cmd), node_id, neighbour_node))
            {
                LOG_ERROR("Failed to add new entanglement request from node " + std::to_string(node_id) + " to neighbour " + std::to_string(neighbour_node) + ".");
            }
        }

        //entanglement_manager->update_requests();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    LOG_INFO("Ending Entanglement Runner.");
    return 0;
}

void EntanglementRunner::on_abort()
{
    // maybe run cleanup
    LOG_INFO("Aborting EntanglementRunner.");
    abort = true;
}

void EntanglementRunner::send_response_async(int node_id, Outcome_Status_t status)
{
    TransportData<ResponseData> transport_data;
    transport_data.node_id = node_id;
    transport_data.destination = DestinationType::CommandRunner;
    transport_data.source = DestinationType::EntanglementRunner;
    transport_data.transport = std::make_unique<ResponseData>();
    transport_data.transport->outcome_status = status;
    message_broker->route_response(std::move(transport_data));
}

// callback for sending command back to the node runner from manager
void EntanglementRunner::send_command_async(int node_id, std::unique_ptr<CommandBase> cmd)
{
    TransportData<CommandBase> transport_data;
    transport_data.node_id = node_id;
    transport_data.destination = DestinationType::SimRunner;
    transport_data.source = DestinationType::EntanglementRunner;
    transport_data.transport = std::move(cmd);
    message_broker->route_command(std::move(transport_data));
}