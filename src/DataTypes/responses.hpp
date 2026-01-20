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

enum Outcome_Status_t
{
    UNKNOWN_FAILURE = 0x00,
    HARDWARE_FAILURE = 0x01,
    UNSUPPORTED_REQUEST = 0x02,
    INVALID_QUBIT = 0x03,
    ENTANGLEMENT_FAILURE = 0x04,
    ENTANGLEMENT_SYNC_FAILURE = 0x05,
    SUCCESS = 0x08
};


enum Measurement_t
{
    MEASURE_OUTCOME_PLUS_1      = 0x8,
    MEASURE_OUTCOME_MINUS_1     = 0xC,
};

enum Entaglement_t
{
    STATE_PHI_PLUS              = 0x8,
    STATE_PHI_MINUS             = 0x9,
    STATE_PSI_PLUS              = 0xA,
    STATE_PSI_MINUS             = 0xB
};

enum Bell_State_Measurent_t
{
    BELL_STATE_PHI_PLUS          = 0x8,
    BELL_STATE_PHI_MINUS         = 0x9,
    BELL_STATE_PSI_PLUS          = 0xA,
    BELL_STATE_PSI_MINUS         = 0xB
};

struct ResponseData
{
    uint8_t status = NodeStatus_t::READY;
    uint8_t outcome_result = 0;
    Outcome_Status_t outcome_status = Outcome_Status_t::SUCCESS;
};
