/*
** Created on: 21.08.2025
** Created by: mrkolev
** Description: Contains all the commands information for QNodeOS
*/

#pragma once
#include <cstdint>

#define CALCULATE_FIDELITY(value)   (50 + (int)value*5)

enum Axis_t
{
    X_AXIS  =   0x0,
    Y_AXIS  =   0x01,
    Z_AXIS  =   0x02
};

enum Angle_t
{

};


enum Command_t
{
    NOP =                               0x00,
    QUBIT_INIT =                        0x01,
    SINGLE_QUBIT_GATE =                 0x02,
    TWO_QUBIT_GATE =                    0x03,
    QUBIT_MEASUREMENT =                 0x04,
    ENTANGLEMENT_ATTEMPT =              0x05,
    ENTANGLEMENT_MEASURE =              0x06,
    PRE_MEASUREMENT_GATES_SELECTION =   0x07,
    QUBIT_MOVE =                        0x08,
    QUBIT_SWAP =                        0x09,
    ENTANGLEMENT_SWAP =                 0x0A
};

struct BasicCommand
{
    BasicCommand(const uint8_t* &data)
    {
        // check bit order
        crc = data[3];
        command_type = static_cast<Command_t>(data[0]);
    }
    Command_t command_type;
    uint8_t crc;
};

struct QubitInitialize: BasicCommand
{
    QubitInitialize(const uint8_t* &data) : BasicCommand(data)
    {
        qubit1_address = 0x0f & data[1];
    }
    uint8_t qubit1_address;
};

//must write down all commands