#pragma once
#include <gmock/gmock.h>
#include "CommandHandler/transmision_handler.hpp"

struct MockTransmissionHandler : ITransmissionHandler {
    MOCK_METHOD(int8_t, receive_data, (uint8_t* buffer, uint8_t length), (override));
    MOCK_METHOD(int8_t, send_data,    (const uint8_t* buffer, uint8_t length), (override));
};
