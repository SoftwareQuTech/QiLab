#include <thread>
#include <iostream>
#include <chrono>

#include "transmission_runner.hpp"

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#define CPU_PAUSE() _mm_pause()
#elif defined(__aarch64__)
#define CPU_PAUSE() asm volatile("yield" ::: "memory")
#else
#define CPU_PAUSE() ((void)0)
#endif

DataTransmissionRunner::DataTransmissionRunner(std::vector<int16_t> nodes, std::unique_ptr<TransmissionProcessor> processor, std::shared_ptr<MessageBroker> broker) : transmission_processor(std::move(processor)), registered_nodes(nodes), message_broker(broker) {}

DataTransmissionRunner::~DataTransmissionRunner() {}

#include "utils/thread_utils.hpp"

int DataTransmissionRunner::on_run(int core_id)
{
    // Set affinity at the very start
    if (core_id >= 0) {
        // Try to pin thread and verify success
        if (!thread_utils::pin_current_thread_to_core(core_id)) {
            std::cerr << "WARNING: Failed to properly pin TransmissionRunner to core " 
                      << core_id << std::endl;
        }

        // Double check after a small delay
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        int current_core = sched_getcpu();
        if (current_core != core_id) {
            std::cerr << "WARNING: TransmissionRunner still not on correct core. "
                      << "Expected " << core_id << " but on " << current_core 
                      << std::endl;
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
                // std::cout<< "Sending response [ "  << std::to_string(response.node_id) << " ] : "<< std::to_string(response.transport->outcome_result) << std::endl;
                responses[response.node_id] = std::move(response.transport);
            }
        }
        else
        {
            // responses[0] = std::move(test_resp);
        }
        PROF_STOP(prof_event_serialize);
        PROF_START(prof_event_spi_transfer);

        int ret_c = transmission_processor->transmit_msg(responses, commands, registered_nodes.size());
        if (ret_c == -1)
        {
            // add error logic
        }
        else if (ret_c == -2)
        {
            // add error logic
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
            command_context.destination =
                (commands[i]->command_type == Command_t::ENTANGLEMENT_ATTEMPT ||
                 commands[i]->command_type == Command_t::ENTANGLEMENT_MEASURE)
                    ? DestinationType::EntanglementRunner
                    : DestinationType::SimRunner;
            command_context.transport = std::move(commands[i]);

            message_broker->route_command(std::move(command_context));
        }
        //CPU_PAUSE();
        std::this_thread::sleep_for(std::chrono::microseconds(50));
        PROF_STOP(prof_event_deserialize);
        //std::this_thread::yield();
    }
    PROF_THREAD_FLUSH();
    // TODO: add clean exit
    return 0;
}

void DataTransmissionRunner::on_abort()
{
    // TODO: see if some cleanup is needed!
    abort = true;
}