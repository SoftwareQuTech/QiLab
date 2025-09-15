/*
** Created on: 21.08.2025
** Created by: mrkolev
** Description: Contains all the commands information for QNodeOS
*/

#pragma once
#include <cstdint>


//double check the bit masks@@
#define ANGLE_BIT_MASK      0x1f
#define AXIS_BIT_MASK       0x60
#define QUBIT_1_BIT_MASK    0x0f
#define QUBIT_2_BIT_MASK    0xf0
#define BASIS_BIT_MASK      0x01
#define QID_BIT_MASK        0x0c
#define NGHBR_BIT_MASK      0xf0
#define FIDELITY_BIT_MASK   0x0f


#define CALCULATE_FIDELITY(value)   (50 + (int)value*5)

enum Axis_t
{
    X_AXIS  =   0x0,
    Y_AXIS  =   0x01,
    Z_AXIS  =   0x02
};

enum Angle_t
{
    _PI_16 = 1,
    _2_PI_16 = 2,
    _3_PI_16= 3,
    _4_PI_16= 4,
    _5_PI_16= 5,
    _6_PI_16= 6,
    _7_PI_16= 7,
    _8_PI_16= 8,
    _9_PI_16= 9,
    _10_PI_16= 10,
    _11_PI_16= 11,
    _12_PI_16= 12,    
    _13_PI_16 = 13,
    _14_PI_16 = 14,
    _15_PI_16= 15,
    _MINUS_PI= 16,
    _MINUS_15_PI_16= 17,
    _MINUS_14_PI_16= 18,
    _MINUS_13_PI_16= 19,
    _MINUS_12_PI_16= 20,
    _MINUS_11_PI_16= 21,
    _MINUS_10_PI_16= 22,
    _MINUS_9_PI_16= 23,
    _MINUS_8_PI_16= 24,
    _MINUS_7_PI_16= 25,
    _MINUS_6_PI_16= 26,
    _MINUS_5_PI_16=27,
    _MINUS_4_PI_16= 28,
    _MINUS_3_PI_16= 29,
    _MINUS_2_PI_16= 30,
    _MINUS_1_PI_16= 31
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
    ENTANGLEMENT_SWAP =                 0x0A,
    INVALID_COMMAND =                   0xFF
};

struct CommandBase
{
    CommandBase(const uint8_t* &data)
    {
        // check bit order
        crc = data[3];
        command_type = static_cast<Command_t>(data[0]);
    }
    virtual ~CommandBase() {}
    Command_t command_type;
    uint8_t crc;
};

struct NOP_Command final: public CommandBase
{
    NOP_Command(const uint8_t* &data) : CommandBase(data)
    {
    }
};

struct QubitInitialize final: public CommandBase
{
    QubitInitialize(const uint8_t* &data) : CommandBase(data)
    {
        qubit1_address = QUBIT_1_BIT_MASK & data[1];
    }
    uint8_t qubit1_address;
};

struct SingleQubitGate final: public CommandBase
{
    SingleQubitGate(const uint8_t* &data) : CommandBase(data)
    {
        qubit1_address = (QUBIT_1_BIT_MASK & data[1]);
        angle = static_cast<Angle_t>((ANGLE_BIT_MASK & data[2]));
        axis = static_cast<Axis_t>((AXIS_BIT_MASK & data[2]) >> 5); //check
    }
    uint8_t qubit1_address;
    Angle_t angle;
    Axis_t axis;
};

struct TwoQubitGate final: public CommandBase
{
    TwoQubitGate(const uint8_t* &data) : CommandBase(data)
    {
        qubit1_address = (QUBIT_1_BIT_MASK & data[1]);
        qubit2_address = (QUBIT_2_BIT_MASK & data[1]) >> 4;
        angle = static_cast<Angle_t>((ANGLE_BIT_MASK & data[2]));
        axis = static_cast<Axis_t>((AXIS_BIT_MASK & data[2]) >> 5);
    }
    uint8_t qubit1_address;
    uint8_t qubit2_address;
    Angle_t angle;
    Axis_t axis;
};

struct QubitMeasure final: public CommandBase
{
    QubitMeasure(const uint8_t* &data) : CommandBase(data)
    {
        qubit1_address = QUBIT_1_BIT_MASK & data[1];
        basis = (BASIS_BIT_MASK & data[2]);
    }
    uint8_t qubit1_address;
    uint8_t basis;
};

struct EntanglementAttempt final: public CommandBase
{
    EntanglementAttempt(const uint8_t* &data) : CommandBase(data)
    {
        fidelity = (FIDELITY_BIT_MASK & data[1]);
        nghbr = (NGHBR_BIT_MASK & data[1]) >> 4;
        qid = ((QID_BIT_MASK & data[2]) >> 2);
    }
    uint8_t fidelity;
    uint8_t nghbr;
    uint8_t qid;
};

struct EntangleMeasure final: public CommandBase
{
    EntangleMeasure(const uint8_t* &data) : CommandBase(data)
    {
        fidelity = (FIDELITY_BIT_MASK & data[1]);
        nghbr = (NGHBR_BIT_MASK & data[1]) >> 4;
        qid = ((QID_BIT_MASK & data[2]) >> 2);
        basis = (BASIS_BIT_MASK & data[2]);
        immediate = (0x02 & data[2]) >> 1;
        repetitions = (0xf0 & data[2]) >> 4;
    }
    uint8_t fidelity;
    uint8_t nghbr;
    uint8_t qid;
    uint8_t basis;
    bool immediate;
    uint8_t repetitions;
};

struct PreMeasureGatesSelection final: public CommandBase
{
    PreMeasureGatesSelection(const uint8_t* &data) : CommandBase(data)
    {
        angle1 = static_cast<Angle_t>(0x1f & data[1]);
        angle2 = static_cast<Angle_t>(((0x03 & data[2]) << 3) | ((0xe0 & data[1]) >> 5));
        angle3 = static_cast<Angle_t>((0x7c & data[2]) >> 2);
        target = (0x80 & data[2]) >> 7;
    }
    Angle_t angle1;
    Angle_t angle2;
    Angle_t angle3;
    bool target;
};

struct QubitMove final: public CommandBase
{
    QubitMove(const uint8_t* &data) : CommandBase(data)
    {
        qubit1_address = QUBIT_1_BIT_MASK & data[1];
        qubit2_address = (QUBIT_2_BIT_MASK & data[1]) >> 4;
    }
    uint8_t qubit1_address;
    uint8_t qubit2_address;
};

struct QubitSwap final: public CommandBase
{
    QubitSwap(const uint8_t* &data) : CommandBase(data)
    {
        qubit1_address = QUBIT_1_BIT_MASK & data[1];
        qubit2_address = (QUBIT_2_BIT_MASK & data[1]) >> 4;
    }
    uint8_t qubit1_address;
    uint8_t qubit2_address;
};

struct EntanglementSwap final: public CommandBase
{
    EntanglementSwap(const uint8_t* &data) : CommandBase(data)
    {
        qubit1_address = QUBIT_1_BIT_MASK & data[1];
        qubit2_address = (QUBIT_2_BIT_MASK & data[1]) >> 4;
    }
    uint8_t qubit1_address;
    uint8_t qubit2_address;
};