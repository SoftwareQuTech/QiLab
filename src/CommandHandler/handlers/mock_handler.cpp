#include <algorithm>

#include "mock_handler.hpp"
#include "DataTypes/commands.hpp"

#define EXPECTED_MESSAGE_SIZE       4

MockHandler::MockHandler()
{
    create_commands();
}
MockHandler::MockHandler(ILogger* logger) : local_logger(logger) 
{
    create_commands();
}
MockHandler::~MockHandler(){}

void MockHandler::create_commands()
{
    command_list.push_back(new Nop());
    command_list.push_back(new InitQubit(0));
    command_list.push_back(new SingleQGate(0, Angle_t::_15_PI_16, Axis_t::Y_AXIS));
}

void MockHandler::fill_mock_message(uint8_t* output)
{
    static int message_num = 0;

    command_list[message_num]->create_command(output);

    if(message_num == 6)
        message_num = 0;

    //std::copy_n(messages[message_num++], 4, output);
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


// The following section contains helpers that produce commands on demand
// This is useful to compile the command as byte array and keep the system as is
// and in the same time make it easier until we plug in the proper device


struct Nop: public BaseC
{
    Nop() {}
    void create_command(uint8_t *buff) override
    {
        buff[0] = Command_t::NOP;
        buff[1] = 0x0;
        buff[2] = 0x0;
        uint8_t crc = crc8_ccitt(buff, 3);
        buff[3] = crc;
    }
};

struct InitQubit: public BaseC
{
    InitQubit(uint8_t q_addr): addr(q_addr) {}
    uint8_t addr;
    void create_command(uint8_t *buff) override
    {
        buff[0] = Command_t::QUBIT_INIT;
        buff[1] = QUBIT_1_BIT_MASK & addr;
        buff[2] = 0x0;
        uint8_t crc = crc8_ccitt(buff, 3);
        buff[3] = crc;
    }
};

struct SingleQGate: public BaseC
{
    SingleQGate(uint8_t q_addr, Angle_t angle, Axis_t axis): addr(q_addr), ax(axis), an(angle) {}
    uint8_t addr;
    uint8_t ax;
    uint8_t an;

    void create_command(uint8_t *buff) override
    {
        buff[0] = Command_t::QUBIT_INIT;
        buff[1] = QUBIT_1_BIT_MASK & addr;
        buff[2] = ((ax << 5) & AXIS_BIT_MASK) | (an & ANGLE_BIT_MASK);
        uint8_t crc = crc8_ccitt(buff, 3);
        buff[3] = crc;
    }
};

struct TwoQGate: public BaseC
{
    TwoQGate(uint8_t q1_addr, uint8_t q2_addr, Angle_t angle, Axis_t axis): addr1(q1_addr), addr2(q2_addr), ax(axis), an(angle) {}
    uint8_t addr1;
    uint8_t addr2;
    uint8_t ax;
    uint8_t an;

    void create_command(uint8_t *buff) override
    {
        buff[0] = Command_t::QUBIT_INIT;
        buff[1] = (QUBIT_1_BIT_MASK & addr1) | ((addr2 << 4) & QUBIT_2_BIT_MASK);
        buff[2] = ((ax << 5) & AXIS_BIT_MASK) | (an & ANGLE_BIT_MASK);
        uint8_t crc = crc8_ccitt(buff, 3);
        buff[3] = crc;
    }
};

struct QMeasure: public BaseC
{
    QMeasure(uint8_t q_addr, bool basis, Axis_t axis): addr(q_addr), b(basis) {}
    uint8_t addr;
    bool b;

    void create_command(uint8_t *buff) override
    {
        buff[0] = Command_t::QUBIT_INIT;
        buff[1] = QUBIT_1_BIT_MASK & addr;
        buff[2] = b & BASIS_BIT_MASK;
        uint8_t crc = crc8_ccitt(buff, 3);
        buff[3] = crc;
    }
};

struct Entangle: public BaseC
{
    Entangle(uint8_t fidelity, uint8_t nghbr, uint8_t qid): f(fidelity), n(nghbr), q(qid) {}
    uint8_t f;
    uint8_t n;
    uint8_t q;

    void create_command(uint8_t *buff) override
    {
        buff[0] = Command_t::QUBIT_INIT;
        buff[1] = ((n << 4) & NGHBR_BIT_MASK) | (FIDELITY_BIT_MASK & f);
        buff[2] = (q << 2) & QID_BIT_MASK;
        uint8_t crc = crc8_ccitt(buff, 3);
        buff[3] = crc;
    }
};

struct EntangleMeasure: public BaseC
{
    EntangleMeasure(uint8_t fidelity, uint8_t nghbr, uint8_t qid, bool try_now, bool basis, uint8_t repeat): 
        f(fidelity), n(nghbr), q(qid), now(try_now), b(basis), r(repeat) {}
    uint8_t f;
    uint8_t n;
    uint8_t q;
    bool now;
    bool b;
    uint8_t r;

    void create_command(uint8_t *buff) override
    {
        buff[0] = Command_t::QUBIT_INIT;
        buff[1] = ((n << 4) & NGHBR_BIT_MASK) | (FIDELITY_BIT_MASK & f);
        buff[2] = ((r << 4) & 0xf0) | ((q << 2) & QID_BIT_MASK) | ((now & 0x01) << 1) | (b & BASIS_BIT_MASK);
        uint8_t crc = crc8_ccitt(buff, 3);
        buff[3] = crc;
    }
};

struct PreMeasureGateSelect: public BaseC
{
    PreMeasureGateSelect(Angle_t angle1, Angle_t angle2, Angle_t angle3, bool target): 
        a1(angle1), a2(angle2), a3(angle3), t(target) {}
    Angle_t a1;
    Angle_t a2;
    Angle_t a3;
    bool t;

    void create_command(uint8_t *buff) override
    {
        buff[0] = Command_t::PRE_MEASUREMENT_GATES_SELECTION;
        buff[1] =  ((a2 & 0x0e) << 5) |(ANGLE_BIT_MASK & a1);
        buff[2] = ((t << 7) & 0x80) | ((a3 << 2) & 0xec) | ((a2 & 0x18) >> 3);
        uint8_t crc = crc8_ccitt(buff, 3);
        buff[3] = crc;
    }
};

struct QSwap: public BaseC
{
    QSwap(uint8_t q1_addr, uint8_t q2_addr): addr1(q1_addr), addr2(q2_addr) {}
    uint8_t addr1;
    uint8_t addr2;

    void create_command(uint8_t *buff) override
    {
        buff[0] = Command_t::QUBIT_SWAP;
        buff[1] = (QUBIT_1_BIT_MASK & addr1) | ((addr2 << 4) & QUBIT_2_BIT_MASK);
        buff[2] = 0x00;
        uint8_t crc = crc8_ccitt(buff, 3);
        buff[3] = crc;
    }
};

struct QMove: public BaseC
{
    QMove(uint8_t q1_addr, uint8_t q2_addr): addr1(q1_addr), addr2(q2_addr) {}
    uint8_t addr1;
    uint8_t addr2;

    void create_command(uint8_t *buff) override
    {
        buff[0] = Command_t::QUBIT_MOVE;
        buff[1] = (QUBIT_1_BIT_MASK & addr1) | ((addr2 << 4) & QUBIT_2_BIT_MASK);
        buff[2] = 0x00;
        uint8_t crc = crc8_ccitt(buff, 3);
        buff[3] = crc;
    }
};

struct EntangleSwap: public BaseC
{
    EntangleSwap(uint8_t q1_addr, uint8_t q2_addr): addr1(q1_addr), addr2(q2_addr) {}
    uint8_t addr1;
    uint8_t addr2;

    void create_command(uint8_t *buff) override
    {
        buff[0] = Command_t::ENTANGLEMENT_SWAP;
        buff[1] = (QUBIT_1_BIT_MASK & addr1) | ((addr2 << 4) & QUBIT_2_BIT_MASK);
        buff[2] = 0x00;
        uint8_t crc = crc8_ccitt(buff, 3);
        buff[3] = crc;
    }
};
