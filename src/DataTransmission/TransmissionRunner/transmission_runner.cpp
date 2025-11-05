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


DataTransmissionRunner::DataTransmissionRunner(std::vector<int16_t> nodes, std::shared_ptr<TransmissionProcessor> processor, std::shared_ptr<MessageBroker> broker) : 
    transmission_processor(processor), registered_nodes(nodes), message_broker(broker){}

DataTransmissionRunner::~DataTransmissionRunner() {}


int DataTransmissionRunner::on_run()
{
    ProfGuard pg{};
    std::vector<std::unique_ptr<CommandBase>> commands;
    std::vector<std::unique_ptr<ResponseData>> responses;

    commands.reserve(registered_nodes.size()); //keeps the vector constantly allocated
    responses.reserve(registered_nodes.size()); //keeps the vector constantly allocated
    
    for(auto i = 0; i < registered_nodes.size(); i++)
    {
        commands.push_back(nullptr);
        responses.push_back(std::make_unique<ResponseData>());
    }


    while (!abort) 
    {
        TransportData<CommandBase> command_context;
        //PROF_START(prof_event_handler_thread_timing);

        //initiate transfer with or without messages to send
        TransportData<ResponseData> response;
        if (message_broker->get_response(response))
        {
            if (response.node_id < responses.size())
            {
                //std::cout<< "Sending response [ "  << std::to_string(response.node_id) << " ] : "<< std::to_string(response.transport->outcome_result) << std::endl;
                responses[response.node_id] = std::move(response.transport);
            }
        }
        else
        {
            //responses[0] = std::move(test_resp);
        }

        int ret_c = transmission_processor->transmit_msg(responses, commands, registered_nodes.size());
        if (ret_c == -1)
        {
            // add error logic
        }
        else if (ret_c == -2)
        {
            // add error logic
        }
        for(auto i = 0; i < commands.size(); i++)
        {
            std::unique_ptr<ResponseData> test_resp = std::make_unique<ResponseData>();

            if(commands[i] == nullptr || commands[i]->command_type == Command_t::NOP)
            {
                // add error logic
                // std::cout<< "Received NOP command" << std::endl;
                continue;
            }
            // if (commands[i]->command_type == Command_t::QUBIT_MEASUREMENT)
            // {
            //     test_resp->outcome_result = 0x11;
            //     test_resp->outcome_status = Outcome_Status_t::SUCCESS;
            //     test_resp->status = NodeStatus_t::BUSY_LINK;

            //     responses[0] = std::move(test_resp);
            // }
            // else if (commands[i]->command_type == Command_t::SINGLE_QUBIT_GATE)
            // {
            //     test_resp->outcome_result = 0x22;
            //     test_resp->outcome_status = Outcome_Status_t::SUCCESS;
            //     test_resp->status = NodeStatus_t::BUSY_LINK;

            //     responses[0] = std::move(test_resp);
            // }
            // else{
            //     responses[0] = std::move(test_resp);
            // }
            // send to broker
            if (commands[i]->command_type == Command_t::ENTANGLEMENT_ATTEMPT || commands[i]->command_type == Command_t::ENTANGLEMENT_MEASURE)
            {
                command_context.node_id = registered_nodes[i]; //TODO: map node ids to commands
                command_context.transport = std::move(commands[i]);
                command_context.destination = DestinationType::EntanglementRunner;
                command_context.source = DestinationType::CommandRunner;
                message_broker->route_command(std::move(command_context));
            }
            else
            {
                command_context.node_id = registered_nodes[i]; //TODO: map node ids to commands
                command_context.transport = std::move(commands[i]);
                command_context.destination = DestinationType::SimRunner;
                command_context.source = DestinationType::CommandRunner;
                message_broker->route_command(std::move(command_context));
            }
        }
        //std::this_thread::sleep_for(std::chrono::microseconds(800));
        std::this_thread::yield();
        //PROF_STOP(prof_event_handler_thread_timing);
    }
    PROF_THREAD_FLUSH();
    //TODO: add clean exit
    return 0;
}

void DataTransmissionRunner::on_abort()
{
    //TODO: see if some cleanup is needed!
    abort = true;
}