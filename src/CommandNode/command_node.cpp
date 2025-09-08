#include "command_node.hpp"

CommandNode::CommandNode(int16_t id, CommandHandler *handler, std::function<void(const CommandData_t &)> cb) : node_id(id), command_handler(handler),
                                                                                                               simulator_command_data_notify(std::move(cb))
{
}
CommandNode::CommandNode(int16_t id, CommandHandler *handler, std::function<void(const CommandData_t &)> cb, IProfiler *logger) : node_id(id), command_handler(handler),
                                                                                                                                  simulator_command_data_notify(std::move(cb)),
                                                                                                                                  local_logger(logger)
{
}

CommandNode::~CommandNode() {}

void CommandNode::run_node()
{
    while (!abort) 
    {
        CommandData_t command_context;
        command_context.node_id = node_id;
        command_context.response_callback = std::bind(&CommandNode::update_response_data, this, std::placeholders::_1);
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
        command_context.command = command.get();
        simulator_command_data_notify(command_context);
        {
            std::lock_guard<std::mutex> lock(response_data_lock); //lock in case sim updates
            command_handler->send_response(current_response);
        }
    }

    //TODO: add clean exit
}

void CommandNode::stop_node()
{
    //TODO: see if some cleanup is needed!
    abort = true;
}

void CommandNode::update_response_data(const ResponseData &response_data)
{
    std::lock_guard<std::mutex> lock(response_data_lock); //lock while update
    current_response = response_data;
}