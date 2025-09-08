/*
** Created on: 08.09.2025
** Created by: mrkolev
** Description: Contains the Command Node Logic
*/

#pragma once
#include <functional>
#include <mutex>

#include "DataTypes/command_data.hpp"
#include "DataTypes/responses.hpp"
#include "CommandHandler/command_handler.hpp"
class CommandNode
{
    CommandNode(int16_t id, CommandHandler *handler, std::function<void(const CommandData_t&)> cb);
    CommandNode(int16_t id, CommandHandler *handler, std::function<void(const CommandData_t&)> cb, IProfiler*);
    ~CommandNode();
    
    void run_node();
    void stop_node();

    private:
    bool abort = false;
    std::function<void(const CommandData_t&)> simulator_command_data_notify;
    int16_t node_id = -1;
    std::mutex response_data_lock;
    ResponseData current_response;
    CommandHandler* command_handler;
    IProfiler* local_logger;
    void update_response_data(const ResponseData&);
};