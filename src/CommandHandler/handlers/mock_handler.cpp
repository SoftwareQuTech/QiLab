#include <algorithm>

#include "mock_handler.hpp"
#include "DataTypes/commands.hpp"

#define EXPECTED_MESSAGE_SIZE       4

MockHandler::MockHandler()
{
    create_commands();
}
MockHandler::MockHandler(IProfiler* logger) : local_logger(logger) 
{
    create_commands();
}
MockHandler::~MockHandler(){}

void MockHandler::create_commands()
{
    command_list.push_back(new InitQubit(0));
    command_list.push_back(new InitQubit(1));
    command_list.push_back(new InitQubit(2));
    command_list.push_back(new SingleQGate(0, Angle_t::_15_PI_16, Axis_t::X_AXIS));
    command_list.push_back(new SingleQGate(0, Angle_t::_8_PI_16, Axis_t::Y_AXIS));
    command_list.push_back(new TwoQGate(0, 1, Angle_t::_8_PI_16, Axis_t::X_AXIS));
    command_list.push_back(new TwoQGate(1, 2, Angle_t::_8_PI_16, Axis_t::X_AXIS));    
    command_list.push_back(new QMeasure(0, 0, Axis_t::Z_AXIS));
    command_list.push_back(new QMeasure(1, 0, Axis_t::Z_AXIS));
    command_list.push_back(new QMeasure(2, 0, Axis_t::Z_AXIS));
}

void MockHandler::fill_mock_message(uint8_t* output)
{
    command_list[message_num++]->create_command(output, 4);

    if(message_num == command_list.size())
        message_num = 0;

    //std::copy_n(messages[message_num++], 4, output);
}


int8_t MockHandler::send_data(const uint8_t* data_to_send, uint8_t size)
{
    //no where to send at  the moment. so just log it
    //LOG(data to send)
    //local_logger->log_event("EVENT DATA", std::to_string(data_to_send[1]));
    return -1;
}


int8_t MockHandler::receive_data(uint8_t* received_data, uint8_t size)
{
    if(size > EXPECTED_MESSAGE_SIZE)
    {
        return -1;
    }
    fill_mock_message(received_data);

    return 0;
}
