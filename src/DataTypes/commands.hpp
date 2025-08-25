/*
** Created on: 21.08.2025
** Created by: mrkolev
** Description: Contains all the commands information for QNodeOS
*/

#pragma once
#include <cstdint>

#define ANGLE_BIT_MASK      0x1f
#define AXIS_BIT_MASK       0x03
#define QUBIT_BIT_MASK      0x0f
#define BASIS_BIT_MASK      0x01
#define QID_BIT_MASK        0x03
#define NGHBR_BIT_MASK      0x0f
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
        qubit1_address = 0x0f & data[1];
    }
    uint8_t qubit1_address;
};

struct SingleQubitGate final: public CommandBase
{
    SingleQubitGate(const uint8_t* &data) : CommandBase(data)
    {
        qubit1_address = (QUBIT_BIT_MASK & data[1]);
        angle = (ANGLE_BIT_MASK & data[2]);
        axis = AXIS_BIT_MASK & (data[2] >> 5);
    }
    uint8_t qubit1_address;
    uint8_t angle;
    uint8_t axis;
};

struct TwoQubitGate final: public CommandBase
{
    TwoQubitGate(const uint8_t* &data) : CommandBase(data)
    {
        qubit1_address = (QUBIT_BIT_MASK & data[1]);
        qubit2_address = QUBIT_BIT_MASK & (data[1] >> 4);
        angle = (ANGLE_BIT_MASK & data[2]);
        axis = AXIS_BIT_MASK & (data[2] >> 5);
    }
    uint8_t qubit1_address;
    uint8_t qubit2_address;
    uint8_t angle;
    uint8_t axis;
};

struct QubitMeasure final: public CommandBase
{
    QubitMeasure(const uint8_t* &data) : CommandBase(data)
    {
        qubit1_address = QUBIT_BIT_MASK & data[1];
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
        nghbr = NGHBR_BIT_MASK & (data[1] >> 4);
        qid = QID_BIT_MASK & (data[2] >> 2);
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
        nghbr = NGHBR_BIT_MASK & (data[1] >> 4);
        qid = QID_BIT_MASK & (data[2] >> 2);
        basis = (BASIS_BIT_MASK & data[2]);
        immediate = 0x01 & (data[2] >> 1);
        repetitions = 0x0f & (data[2] >> 4);
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
        angle1 = (0x1f & data[1]);
        angle2 = (0x07 & (data[1] >> 5)) + (0x18 & (data[2] << 3));
        angle2 = ANGLE_BIT_MASK & (data[2] >> 2);
        target = 0x01 & (data[2] >> 7);
    }
    uint8_t angle1;
    uint8_t angle2;
    uint8_t angle3;
    bool target;
};

struct QubitMove final: public CommandBase
{
    QubitMove(const uint8_t* &data) : CommandBase(data)
    {
        qubit1_address = QUBIT_BIT_MASK & data[1];
        qubit2_address = QUBIT_BIT_MASK & (data[1] >> 4);
    }
    uint8_t qubit1_address;
    uint8_t qubit2_address;
};

struct QubitSwap final: public CommandBase
{
    QubitSwap(const uint8_t* &data) : CommandBase(data)
    {
        qubit1_address = QUBIT_BIT_MASK & data[1];
        qubit2_address = QUBIT_BIT_MASK & (data[1] >> 4);
    }
    uint8_t qubit1_address;
    uint8_t qubit2_address;
};

struct EntanglementSwap final: public CommandBase
{
    EntanglementSwap(const uint8_t* &data) : CommandBase(data)
    {
        qubit1_address = QUBIT_BIT_MASK & data[1];
        qubit2_address = QUBIT_BIT_MASK & (data[1] >> 4);
    }
    uint8_t qubit1_address;
    uint8_t qubit2_address;
};