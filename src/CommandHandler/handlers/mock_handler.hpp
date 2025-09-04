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
    uint8_t receive_data(uint8_t *, uint8_t) override;
    uint8_t send_data(const uint8_t *, uint8_t) override;

private:
    void fill_mock_message(uint8_t*);
    void create_commands();
    IProfiler* local_logger;
    std::vector<BaseC*> command_list;
    MockHandler(const MockHandler &) = delete;
    MockHandler(const MockHandler &&) = delete;
    MockHandler &operator=(MockHandler &) = delete;
    MockHandler &operator=(MockHandler &&) = delete;
};