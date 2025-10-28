/*
** Created on: 25.08.2025
** Created by: mrkolev
** Description: Contains the implementation for a mock command handler.
**              The idea is fixed commands can be send in an uniform program
*/

#pragma once
#include "transmision_handler.hpp"
#include "command_helper.hpp"

class MockHandler final : public ITransmissionHandler
{
public:
    MockHandler();
    ~MockHandler();
    int8_t receive_data(uint8_t *, uint8_t) override;
    int8_t send_data(const uint8_t *, uint8_t) override;
    void flush() override {};
    int8_t transfer(uint8_t const* tx_buffer, uint8_t const* rx_buffer, const uint8_t length) override;
private:
    void fill_mock_message(uint8_t*);
    void create_commands();
    int latest_measure_response = -1;
    std::vector<BaseC*> command_list;
    
    int message_num = 0;
    
    MockHandler(const MockHandler &) = delete;
    MockHandler(const MockHandler &&) = delete;
    MockHandler &operator=(MockHandler &) = delete;
    MockHandler &operator=(MockHandler &&) = delete;
};