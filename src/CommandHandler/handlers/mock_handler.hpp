/*
** Created on: 25.08.2025
** Created by: mrkolev
** Description: Contains the implementation for a mock command handler.
**              The idea is fixed commands can be send in an uniform program
*/

#pragma once
#include "CommandHandler/transmision_handler.hpp"
#include "logger/logger.hpp"
#include "command_helper.hpp"

class MockHandler final : public ITransmissionHandler
{
public:
    MockHandler();
    MockHandler(IProfiler*);
    ~MockHandler();
    int8_t receive_data(uint8_t *, uint8_t) override;
    int8_t send_data(const uint8_t *, uint8_t) override;

private:
    void fill_mock_message(uint8_t*);
    void create_commands();
    int latest_measure_response = -1;
    IProfiler* local_logger;
    std::vector<BaseC*> command_list;

<<<<<<< HEAD
=======
    int message_num = 0;

>>>>>>> 363b535 (make multithreaded work. Refactored some pointer data in order to own it properly. REMOVED static from mocked transport(bug). tested with 2 threads, they operate properly with results that are the same as one thread)
    MockHandler(const MockHandler &) = delete;
    MockHandler(const MockHandler &&) = delete;
    MockHandler &operator=(MockHandler &) = delete;
    MockHandler &operator=(MockHandler &&) = delete;
};