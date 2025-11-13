#include <algorithm>

#include "mock_handler.hpp"
#include "DataTypes/commands.hpp"

#define EXPECTED_MESSAGE_SIZE 4

MockHandler::MockHandler()
{
    create_commands();
}

MockHandler::~MockHandler()
{
    for (auto c : command_list)
    {
        delete c;
    }
}

void MockHandler::create_commands()
{
    const int number_of_nodes = 3;

    for (auto y = 0; y < (2 * number_of_nodes) - 2; y++)
    {
        command_list.push_back(new InitQubit(y));
    }
    command_list.push_back(new SingleQGate(1, Angle_t::_15_PI_16, Axis_t::X_AXIS));
    command_list.push_back(new Entangle(1, 1, 0));
    int i = 0;
    for (i = 1; i < (number_of_nodes - 1); i++)
    {
        // run the loop here with commands
        command_list.push_back(new SingleQGate(2 * i + 1, Angle_t::_15_PI_16, Axis_t::X_AXIS));
        command_list.push_back(new Entangle(1, 2 * i + 1, 2 * i));
        command_list.push_back(new EntangleSwap(2 * i - 1, 2 * i));
        command_list.push_back(new QMeasure(2 * i - 1, 0, Axis_t::Z_AXIS));
        command_list.push_back(new ConditionalCommand([](const int &latest_measure_response)
                                                      { return latest_measure_response == 1; },
                                                      new SingleQGate(2 * i + 1, Angle_t::_15_PI_16, Axis_t::Z_AXIS)));
        command_list.push_back(new QMeasure(2 * i, 0, Axis_t::Z_AXIS));
        command_list.push_back(new ConditionalCommand([](const int &latest_measure_response)
                                                      { return latest_measure_response == 0; },
                                                      new SingleQGate(2 * i + 1, Angle_t::_15_PI_16, Axis_t::X_AXIS)));
    }
    command_list.push_back(new QMeasure(2 * i - 1, 0, Axis_t::Z_AXIS));
    command_list.push_back(new QMeasure(0, 0, Axis_t::Z_AXIS));
}

void MockHandler::fill_mock_message(uint8_t *output)
{
    // if(local_logger != nullptr)
    //     local_logger->log_event("Using Response: " + std::to_string(latest_measure_response));
    if (!command_list.empty())
    {
        if (command_list[message_num]->conditional_execution(latest_measure_response))
        {
            command_list[message_num]->create_command(output, 4);
        }

        message_num++;
        latest_measure_response = -1;

        if (message_num == command_list.size())
            message_num = 0;
    }
}

int8_t MockHandler::send_data(const uint8_t *data_to_send, uint8_t size)
{
    // no where to send at  the moment. so just log it
    if (size > 4)
        return -1;

    // if(local_logger != nullptr)
    //     local_logger->log_event("Updating Response: " + std::to_string(((data_to_send[1] & 0x40) >> 6)));

    latest_measure_response = ((data_to_send[1] & 0x40) >> 6);
    return -1;
}

int8_t MockHandler::receive_data(uint8_t *received_data, uint8_t size)
{
    if (size > EXPECTED_MESSAGE_SIZE)
    {
        return -1;
    }
    fill_mock_message(received_data);

    return 0;
}

int8_t MockHandler::transfer(uint8_t const* tx_buffer, uint8_t const* rx_buffer, const uint8_t length)
{
    if(length != EXPECTED_MESSAGE_SIZE)
    {
        return -1;
    }
    return 0;
}