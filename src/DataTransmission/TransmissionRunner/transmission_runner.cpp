#include <thread>

#include "transmission_runner.hpp"

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#define CPU_PAUSE() _mm_pause()
#elif defined(__aarch64__)
#define CPU_PAUSE() asm volatile("yield" ::: "memory")
#else
#define CPU_PAUSE() ((void)0)
#endif


DataTransmissionRunner::DataTransmissionRunner(std::vector<int16_t> nodes, TransmissionProcessor *processor, std::shared_ptr<MessageBroker> broker) : 
    transmission_processor(processor), registered_nodes(nodes), message_broker(broker){}
DataTransmissionRunner::~DataTransmissionRunner() {}


int DataTransmissionRunner::on_run()
{
    ProfGuard pg{};
    std::vector<std::unique_ptr<CommandBase>> commands;
    std::vector<std::unique_ptr<ResponseData>> responses;

    commands.reserve(registered_nodes.size()); //keeps the vector constantly allocated
    responses.reserve(registered_nodes.size()); //keeps the vector constantly allocated
    
    while (!abort) 
    {
        TransportData<CommandBase> command_context;
        PROF_START(prof_event_handler_thread_timing);
        //initiate transfer with or without messages to send
        TransportData<ResponseData> response;
        // if(broker->get_response(response)){
        //     if(response.node_id < responses.size())
        //{
        //     responses[response.node_id] = std::make_unique<ResponseData>(response.transport);
        // }
        // }
        // else
        // {
            
        // }

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
            if(commands[i] == nullptr || commands[i]->command_type == Command_t::NOP)
            {
                // add error logic
                continue;
            }
            command_context.node_id = registered_nodes[i]; //TODO: map node ids to commands
            command_context.transport = std::move(commands[i]);
            //send to broker
            //broker->send_command(std::move(command_context));
        }

        PROF_STOP(prof_event_handler_thread_timing);
        std::this_thread::yield();
    }

    //TODO: add clean exit
}

void DataTransmissionRunner::on_abort()
{
    //TODO: see if some cleanup is needed!
    abort = true;
}