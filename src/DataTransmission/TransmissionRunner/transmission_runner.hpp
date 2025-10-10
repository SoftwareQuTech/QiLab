/*
** Created on: 08.09.2025
** Created by: mrkolev
** Description: Contains the Command Node Logic
*/

#pragma once
#include <functional>

#include "DataTypes/command_data.hpp"
#include "DataTypes/responses.hpp"
#include "DataTransmission/TransmissionProcessor/transmission_processor.hpp"
#include "DataTypes/commands.hpp"
#include "logger/profiler_api.hpp"
#include "MessageBroker/message_broker.hpp"

class DataTransmissionRunner
{
    public:
    DataTransmissionRunner(std::vector<int16_t> nodes, TransmissionProcessor *processor, std::shared_ptr<MessageBroker> broker); 
    ~DataTransmissionRunner();

    int on_run();
    void on_abort();

    private:
    std::shared_ptr<MessageBroker> message_broker;
    std::vector<int16_t> registered_nodes;
    bool abort = false;
    std::unique_ptr<TransmissionProcessor> transmission_processor;

    //profiling event ids
    Profiler_id prof_event_handler_thread_timing = PROF_REGISTER_EVENT("handler_thread_timing");
};