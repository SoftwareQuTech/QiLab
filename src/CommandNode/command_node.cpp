#include "command_node.hpp"
#include "thread"
#include <iostream>
#include "logger/profiler_api.hpp"


#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#define CPU_PAUSE() _mm_pause()
#elif defined(__aarch64__)
#define CPU_PAUSE() asm volatile("yield" ::: "memory")
#else
#define CPU_PAUSE() ((void)0)
#endif


CommandNode::CommandNode(int16_t id, CommandHandler *handler) : node_id(id), command_handler(handler){}
CommandNode::~CommandNode() {}
void CommandNode::set_notify_callback(std::function<void(TransportData<CommandBase>)> cb)
{
    simulator_command_data_notify = cb;
}
int CommandNode::on_run()
{
    ProfGuard pg{};

    while (!abort) 
    {
        TransportData<CommandBase> command_context;
        PROF_START("handler_thread_timing");
        std::unique_ptr<CommandBase> command;
        int ret_c = command_handler->receive_command(command);
        if (ret_c == -1)
        {
            // add error logic
        }
        else if (ret_c == -2)
        {
            // add error logic
        }
        command_context.transport = std::move(command);
        simulator_command_data_notify(std::move(command_context));
        TransportData<ResponseData> response;
        if(this->lockfree_queue.pop(response)){

            command_handler->send_response(std::move(response.transport));
        }
        else
        {
            CPU_PAUSE();
        }
        PROF_STOP("handler_thread_timing");
    }

    //TODO: add clean exit
}

void CommandNode::on_abort()
{
    //TODO: see if some cleanup is needed!
    abort = true;
}