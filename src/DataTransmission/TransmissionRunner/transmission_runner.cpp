#include <thread>
#include <iostream>
#include <chrono>

#include "transmission_runner.hpp"
#include "logger/logger_api.hpp"


DataTransmissionRunner::DataTransmissionRunner(std::vector<int16_t> nodes, std::unique_ptr<TransmissionProcessor> processor, std::shared_ptr<MessageBroker> broker) : transmission_processor(std::move(processor)), registered_nodes(nodes), message_broker(broker) {}

DataTransmissionRunner::~DataTransmissionRunner() {}

#include "utils/thread_utils.hpp"

int DataTransmissionRunner::on_run(int core_id)
{
    // Set affinity at the very start
    if (core_id >= 0) {
        // Try to pin thread and verify success
        if (!thread_utils::pin_current_thread_to_core(core_id)) 
        {
            LOG_ERROR("Failed to properly pin TransmissionRunner to core " + std::to_string(core_id) + ".");
        }

        // Double check after a small delay
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        int current_core = sched_getcpu();
        if (current_core != core_id) 
        {
            LOG_WARNING("TransmissionRunner still not on correct core. Expected " + std::to_string(core_id) + " but on " + std::to_string(current_core) + ".");
        }
    }

    ProfGuard pg{};
    std::vector<std::unique_ptr<CommandBase>> commands;
    std::vector<std::unique_ptr<ResponseData>> responses;

    commands.reserve(registered_nodes.size());  // keeps the vector constantly allocated
    responses.reserve(registered_nodes.size()); // keeps the vector constantly allocated

    for (auto i = 0; i < registered_nodes.size(); i++)
    {
        commands.push_back(nullptr);
        responses.push_back(std::make_unique<ResponseData>());
    }

    while (!abort)
    {
        PROF_START(prof_event_serialize);
        TransportData<CommandBase> command_context;

        // initiate transfer with or without messages to send
        TransportData<ResponseData> response;
        if (message_broker->get_response(response))
        {
            if (response.node_id < responses.size())
            {
                responses[response.node_id] = std::move(response.transport);
            }
        }

        PROF_STOP(prof_event_serialize);
        PROF_START(prof_event_spi_transfer);

        if(transmission_processor->transmit_msg(responses, commands, registered_nodes.size()) != 0)
        {
            LOG_ERROR("Transmission Failed during SPI transfer.");
            continue;
        }

        PROF_STOP(prof_event_spi_transfer);
        command_context.source = DestinationType::CommandRunner;
        PROF_START(prof_event_deserialize);
        for (auto i = 0; i < commands.size(); i++)
        {
            if (commands[i] == nullptr || commands[i]->command_type == Command_t::NOP)
            {
                continue;
            }

            command_context.node_id = registered_nodes[i];
            if(commands[i]->command_type == Command_t::ENTANGLEMENT_ATTEMPT ||
               commands[i]->command_type == Command_t::ENTANGLEMENT_MEASURE)
            {
                command_context.destination = DestinationType::EntanglementRunner;
                responses[i]->status = NodeStatus_t::BUSY_LINK;
            }
            else
            {
                responses[i]->status = NodeStatus_t::BUSY_CMD;
                command_context.destination = DestinationType::SimRunner;
            }
            command_context.transport = std::move(commands[i]);

            message_broker->route_command(std::move(command_context));
        }

        //
        //CPU_PAUSE();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        PROF_STOP(prof_event_deserialize);
        //std::this_thread::yield();
    }
    PROF_THREAD_FLUSH();
    LOG_INFO("Ending Transmission Runner.");
    return 0;
}

void DataTransmissionRunner::on_abort()
{
    // TODO: see if some cleanup is needed!
    LOG_INFO("Abort request received on Transmission Runner!");
    abort = true;
}