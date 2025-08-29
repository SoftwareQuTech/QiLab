#include <algorithm>

#include "mock_handler.hpp"
#include "DataTypes/commands.hpp"

#define EXPECTED_MESSAGE_SIZE       4


MockHandler::MockHandler(){}
MockHandler::MockHandler(ILogger* logger) : local_logger(logger) {}
MockHandler::~MockHandler(){}


//void create_command(uint8_t qbit1, uint8_t qbit2, command_t command, angle_t angle, axis_t axis)

void nop_command(uint8_t* buff)
{
    buff[0] = Command_t::NOP;
}





void MockHandler::fill_mock_message(uint8_t* output)
{
    static int message_num = 0;
    uint8_t messages[6][4] = {
        //{0x02, 0x00, 0x0f, 0x04},
        {0x02, 0x00, 0x2f, 0x04},
        //{0x03, 0x10, 0x00, 0x04},
        //{0x03, 0x21, 0x00, 0x04},
        {0x04, 0x00, 0x00, 0x04},
        //{0x04, 0x01, 0x00, 0x04},
        //{0x04, 0x02, 0x00, 0x04}
        {0x02, 0x01, 0x2f, 0x04},
        {0x04, 0x01, 0x00, 0x04},        
        {0x02, 0x02, 0x2f, 0x04},
        {0x04, 0x02, 0x00, 0x04}
    };

    if(message_num == 6)
        message_num = 0;

    std::copy_n(messages[message_num++], 4, output);
}


uint8_t MockHandler::send_data(const uint8_t* data_to_send, uint8_t size)
{
    //no where to send at  the moment. so just log it
    //LOG(data to send)
    //local_logger->log_event("EVENT DATA", std::to_string(data_to_send[1]));
    return -1;
}


uint8_t MockHandler::receive_data(uint8_t* received_data, uint8_t size)
{
    if(size > EXPECTED_MESSAGE_SIZE)
    {
        return -1;
    }
    fill_mock_message(received_data);

    return 0;
}


