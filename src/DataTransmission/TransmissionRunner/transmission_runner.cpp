#include <thread>
#include <iostream>
#include <chrono>

#include "transmission_runner.hpp"
#include "logger/logger_api.hpp"
#include "utils/thread_utils.hpp"

#define TRANSMISSION_STEP_DELAY 10  // microseconds

DataTransmissionRunner::DataTransmissionRunner(std::vector<int16_t> nodes, std::unique_ptr<TransmissionProcessor> processor, std::shared_ptr<MessageBroker> broker) 
    : transmission_processor(std::move(processor)), registered_nodes(nodes), message_broker(broker) 
{
    // Initialize per-node transmission data for each registered node
    node_transmission_data.reserve(registered_nodes.size());
    for (size_t i = 0; i < registered_nodes.size(); ++i) {
        node_transmission_data.emplace_back();
    }
}

DataTransmissionRunner::~DataTransmissionRunner() {}


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

    LOG_INFO("Starting Transmission Runner with " + std::to_string(registered_nodes.size()) + " registered nodes.");
    
    while (!abort)
    {
        PROF_START(prof_event_serialize);
        TransportData<CommandBase> command_context;

        // initiate transfer with or without messages to send
        TransportData<ResponseData> response;
        // TODO: Possibly batch receive responses from message broker here instead of per-message in the loop below? 
        // Would require some restructuring of the message broker interface to support batch pops.
        // The problem is that we add artificial delay per node if we do it one by one and we 
        // have the data available from the simulator(possibly have it)
        if (message_broker->get_response(response))
        {
            if (response.node_id < responses.size())
            {
                responses[response.node_id] = std::move(response.transport);
            }
        }

        PROF_STOP(prof_event_serialize);
        PROF_START(prof_event_spi_transfer);

        //LOG_INFO("Sending response data " + std::to_string(static_cast<uint8_t>(responses[0]->status)) + " for node " + std::to_string(registered_nodes[0]) + ".");
        // Pass all node transmission data for batch processing
        if(transmission_processor->transmit_msg(responses, commands, registered_nodes.size(), node_transmission_data) != 0)
        {
            LOG_ERROR("Transmission Failed during SPI transfer.");
            //std::this_thread::sleep_for(std::chrono::milliseconds(TRANSMISSION_STEP_DELAY));
            continue;
        }

        PROF_STOP(prof_event_spi_transfer);
        command_context.source = DestinationType::CommandRunner;
        PROF_START(prof_event_deserialize);
        for (auto i = 0; i < commands.size(); i++)
        {
            if (commands[i] == nullptr || commands[i]->command_type == Command_t::NOP)
            {
                //std::this_thread::sleep_for(std::chrono::milliseconds(TRANSMISSION_STEP_DELAY));   
                responses[i]->status = NodeStatus_t::READY;
                responses[i]->outcome = Outcome_Status_t::SUCCESS;
                continue;
            }

            LOG_INFO("Routing command of type " + std::to_string(static_cast<uint8_t>(commands[i]->command_type)) + " for node " + std::to_string(registered_nodes[i]) + ".");
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
        std::this_thread::sleep_for(std::chrono::microseconds(TRANSMISSION_STEP_DELAY));
        PROF_STOP(prof_event_deserialize);
        //std::this_thread::yield();
    }
    for(size_t node = 0; node < node_transmission_data.size(); ++node)
    {
        const auto &node_data = node_transmission_data[node];
        LOG_INFO("Final cycle count for node: " + std::to_string(node) + ", " + std::to_string(node_data.cycle_count) + ", CRC error count: " + std::to_string(node_data.crc_error_count));
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