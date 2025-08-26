#include <algorithm>

#include "mock_handler.hpp"

#define EXPECTED_MESSAGE_SIZE       4


MockHandler::MockHandler(){}
MockHandler::~MockHandler(){}

void MockHandler::fill_mock_message(uint8_t* output)
{
    static int message_num = 0;
    uint8_t messages[10][4] = {
        {0x01, 0x01, 0x03, 0x04},
        {0x02, 0x01, 0x08, 0x04},
        {0x02, 0x01, 0x08, 0x04},
        {0x02, 0x01, 0x08, 0x04},
        {0x02, 0x01, 0x08, 0x04},
        {0x02, 0x01, 0x08, 0x04},
        {0x01, 0x02, 0x03, 0x04},
        {0x01, 0x02, 0x03, 0x04},
        {0x01, 0x02, 0x03, 0x04},
        {0x01, 0x02, 0x03, 0x04}
    };

    if(message_num == 10)
        message_num = 0;

    std::copy_n(messages[message_num++], 4, output);
}


uint8_t MockHandler::send_data(const uint8_t* data_to_send, uint8_t size)
{
    //no where to send at  the moment. so just log it
    //LOG(data to send)
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


