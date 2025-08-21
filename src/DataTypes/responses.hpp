/*
** Created on: 21.08.2025
** Created by: mrkolev
** Description: Contains all the response information for QNodeOS
*/

#pragma once
#include <cstdint>

enum NodeStatus_t
{
    READY = 0x00,
    OUTCOME_READY = 0x01,
    BUSY_CMD = 0x02,
    BUSY_LINK = 0x04
};

enum Outcome_t
{
    UNKNOWN_FAILURE = 0x00,
    HARDWARE_FAILURE = 0x01,
    UNSUPPORTED_REQUEST = 0x02,
    INVALID_QUBIT = 0x03,
    ENTANGLEMENT_FAILURE = 0x04,
    ENTANGLEMENT_SYNC_FAILURE = 0x05,
    SUCCESS = 0x08
};

struct ResponseData
{
    uint8_t cycle_count;
    NodeStatus_t status;
    uint8_t crc;
    Outcome_t outcome;
    uint8_t lp_data;
    void serialize_data(uint8_t* buffer)
    {
        buffer[0] = cycle_count;
        buffer[1] = (0xff00 & ((uint8_t)outcome << 8)) + (0x00ff & (uint8_t)status);
        buffer[2] = (uint8_t)lp_data & 0x00ff;
        buffer[3] = crc; //shold calculate ofcourse
    }
};
