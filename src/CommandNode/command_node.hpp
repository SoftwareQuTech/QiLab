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
#include "DataTypes/commands.hpp"
#include "SimulatorRunners/runnable.hpp"

class CommandNode: public IRunnable<TransportData<ResponseData>>
{
    public:
    CommandNode(int16_t id, CommandHandler *handler);
    ~CommandNode();
    void set_notify_callback(std::function<void(TransportData<CommandBase>)> cb);

    int on_run();
    void on_abort();

    private:
    bool abort = false;
    std::function<void(TransportData<CommandBase>)> simulator_command_data_notify;
    int16_t node_id = -1;
    ResponseData current_response;
    CommandHandler* command_handler;
};