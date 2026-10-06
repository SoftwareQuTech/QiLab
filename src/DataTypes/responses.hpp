// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

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
    MEASURE_OUTCOME_PLUS_1      = 0x0,
    MEASURE_OUTCOME_MINUS_1     = 0x4,
};

enum Entaglement_t
{
    STATE_PHI_PLUS              = 0x0,
    STATE_PHI_MINUS             = 0x1,
    STATE_PSI_PLUS              = 0x2,
    STATE_PSI_MINUS             = 0x3
};

enum Bell_State_Measurent_t
{
    BELL_STATE_PHI_PLUS          = 0x0,
    BELL_STATE_PHI_MINUS         = 0x1,
    BELL_STATE_PSI_PLUS          = 0x2,
    BELL_STATE_PSI_MINUS         = 0x3
};

struct ResponseData
{
    uint8_t status = NodeStatus_t::READY;
    uint8_t outcome = Outcome_Status_t::SUCCESS;
    uint64_t cycle_count = 0;
};
