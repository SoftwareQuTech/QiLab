/*
** Created on: 25.08.2025
** Created by: mrkolev
** Description: Contains the ITransmission interface
*/

#pragma once
#include <cstdint>

struct ITransmissionHandler
{
    virtual ~ITransmissionHandler() = default; 
    virtual uint8_t receive_data(uint8_t*, uint8_t) = 0;
    virtual uint8_t send_data(const uint8_t*, uint8_t) = 0;
};