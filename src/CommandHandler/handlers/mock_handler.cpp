#include <algorithm>

#include "mock_handler.hpp"
#include "DataTypes/commands.hpp"

#define EXPECTED_MESSAGE_SIZE 4

MockHandler::MockHandler()
{
    create_commands();
}
MockHandler::MockHandler(IProfiler *logger) : local_logger(logger)
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
    // command_list.push_back(new InitQubit(0));
    // command_list.push_back(new InitQubit(1));
    // command_list.push_back(new InitQubit(2));
    // command_list.push_back(new SingleQGate(0, Angle_t::_15_PI_16, Axis_t::X_AXIS));
    // command_list.push_back(new SingleQGate(0, Angle_t::_8_PI_16, Axis_t::Y_AXIS));
    // command_list.push_back(new TwoQGate(0, 1, Angle_t::_8_PI_16, Axis_t::X_AXIS));
    // command_list.push_back(new TwoQGate(1, 2, Angle_t::_8_PI_16, Axis_t::X_AXIS));
    // command_list.push_back(new QMeasure(0, 0, Axis_t::Z_AXIS));
    // command_list.push_back(new QMeasure(1, 0, Axis_t::Z_AXIS));
    // command_list.push_back(new QMeasure(2, 0, Axis_t::Z_AXIS));

    // command_list.push_back(new QMove(1, 2));
    //  for(auto y = 0; y < (2 * number_of_nodes) - 2 ; y++)
    //  {
    //      command_list.push_back(new InitQubit(y));
    //  }
    //  command_list.push_back(new SingleQGate(1, Angle_t::_15_PI_16, Axis_t::X_AXIS));
    //  command_list.push_back(new Entangle(1, 1, 0));
    //  int i = 0;
    //  for (i = 1; i < (number_of_nodes - 1); i++)
    //  {
    //      // run the loop here with commands
    //      command_list.push_back(new SingleQGate(2 * i + 1, Angle_t::_15_PI_16, Axis_t::X_AXIS));
    //      command_list.push_back(new Entangle(1, 2 * i + 1, 2 * i));
    //      command_list.push_back(new EntangleSwap(2 * i - 1, 2 * i));
    //      command_list.push_back(new QMeasure(2 * i - 1, 0, Axis_t::Z_AXIS));
    //      command_list.push_back(new ConditionalCommand([](const int &latest_measure_response)
    //                                                    { return latest_measure_response == 1; },
    //                                                    new SingleQGate(2 * i + 1, Angle_t::_15_PI_16, Axis_t::Z_AXIS)));
    //      command_list.push_back(new QMeasure(2 * i, 0, Axis_t::Z_AXIS));
    //      command_list.push_back(new ConditionalCommand([](const int &latest_measure_response)
    //                                                    { return latest_measure_response == 0; },
    //                                                    new SingleQGate(2 * i + 1, Angle_t::_15_PI_16, Axis_t::X_AXIS)));
    //  }
    //  command_list.push_back(new QMeasure(2 * i - 1 , 0, Axis_t::Z_AXIS));
    //  command_list.push_back(new QMeasure(0, 0, Axis_t::Z_AXIS));
}

void MockHandler::fill_mock_message(uint8_t *output)
{
    static int message_num = 0;
    static int it = 1; // simple way to count itterations
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
    // local_logger->log_event("EVENT DATA", std::to_string(data_to_send[1]));
    if (size > 4)
        return -1;

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
