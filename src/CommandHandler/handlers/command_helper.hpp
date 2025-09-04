/*
** Created on: 03.09.2025
** Created by: mrkolev
** Description: Contains the implementation for a helper classes
**              In order to create commands in serial represenation
**              for easier program assembly
*/

#pragma once
#include <cstdint>

#include "DataTypes/commands.hpp"

/// Base class for all commands.
class BaseC
{
public:
    virtual ~BaseC() = default;

    /// Build the command into `buff` (up to `max_len` bytes).
    /// @return number of bytes written.
    virtual std::size_t create_command(uint8_t *buff, std::size_t max_len) const = 0;

protected:
    // CRC-8/CCITT parameters.
    static constexpr uint8_t CRC8_CCITT_POLY = 0x07u;
    static constexpr uint8_t CRC8_INIT = 0x00u;
    static constexpr uint8_t CRC8_XOROUT = 0x00u;

    static uint8_t crc8_ccitt_update(uint8_t crc, uint8_t byte) noexcept
    {
        crc ^= byte;
        for (uint8_t i = 0; i < 8; ++i)
        {
            crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ CRC8_CCITT_POLY)
                               : static_cast<uint8_t>(crc << 1);
        }
        return crc;
    }
    static uint8_t crc8_ccitt(const uint8_t *data, std::size_t len) noexcept
    {
        uint8_t crc = CRC8_INIT;
        while (len--)
        {
            crc = crc8_ccitt_update(crc, *data++);
        }
        return static_cast<uint8_t>(crc ^ CRC8_XOROUT);
    }
};

struct Nop : public BaseC
{
    Nop() {}
    std::size_t create_command(uint8_t *buff, std::size_t max_len) const
    {

        // payload size is fixed: 4 bytes
        if (max_len < 4 || buff == nullptr)
            return 0;

        buff[0] = Command_t::NOP;
        buff[1] = 0x0;
        buff[2] = 0x0;
        uint8_t crc = crc8_ccitt(buff, 3);
        buff[3] = crc;

        return 4;
    }
};

struct InitQubit : public BaseC
{
    InitQubit(uint8_t q_addr) : addr(q_addr) {}
    uint8_t addr;
    std::size_t create_command(uint8_t *buff, std::size_t max_len) const
    {

        // payload size is fixed: 4 bytes
        if (max_len < 4 || buff == nullptr)
            return 0;

        buff[0] = Command_t::QUBIT_INIT;
        buff[1] = QUBIT_1_BIT_MASK & addr;
        buff[2] = 0x0;
        uint8_t crc = crc8_ccitt(buff, 3);
        buff[3] = crc;

        return 4;
    }
};

struct SingleQGate : public BaseC
{
    SingleQGate(uint8_t q_addr, Angle_t angle, Axis_t axis) : addr(q_addr), ax(axis), an(angle) {}
    uint8_t addr;
    uint8_t ax;
    uint8_t an;

    std::size_t create_command(uint8_t *buff, std::size_t max_len) const
    {

        // payload size is fixed: 4 bytes
        if (max_len < 4 || buff == nullptr)
            return 0;

        buff[0] = Command_t::SINGLE_QUBIT_GATE;
        buff[1] = QUBIT_1_BIT_MASK & addr;
        buff[2] = ((ax << 5) & AXIS_BIT_MASK) | (an & ANGLE_BIT_MASK);
        uint8_t crc = crc8_ccitt(buff, 3);
        buff[3] = crc;

        return 4;
    }
};

struct TwoQGate : public BaseC
{
    TwoQGate(uint8_t q1_addr, uint8_t q2_addr, Angle_t angle, Axis_t axis) : addr1(q1_addr), addr2(q2_addr), ax(axis), an(angle) {}
    uint8_t addr1;
    uint8_t addr2;
    uint8_t ax;
    uint8_t an;

    std::size_t create_command(uint8_t *buff, std::size_t max_len) const
    {

        // payload size is fixed: 4 bytes
        if (max_len < 4 || buff == nullptr)
            return 0;

        buff[0] = Command_t::TWO_QUBIT_GATE;
        buff[1] = (QUBIT_1_BIT_MASK & addr1) | ((addr2 << 4) & QUBIT_2_BIT_MASK);
        buff[2] = ((ax << 5) & AXIS_BIT_MASK) | (an & ANGLE_BIT_MASK);
        uint8_t crc = crc8_ccitt(buff, 3);
        buff[3] = crc;

        return 4;
    }
};

struct QMeasure : public BaseC
{
    QMeasure(uint8_t q_addr, bool basis, Axis_t axis) : addr(q_addr), b(basis) {}
    uint8_t addr;
    bool b;

    std::size_t create_command(uint8_t *buff, std::size_t max_len) const
    {

        // payload size is fixed: 4 bytes
        if (max_len < 4 || buff == nullptr)
            return 0;

        buff[0] = Command_t::QUBIT_MEASUREMENT;
        buff[1] = QUBIT_1_BIT_MASK & addr;
        buff[2] = b & BASIS_BIT_MASK;
        uint8_t crc = crc8_ccitt(buff, 3);
        buff[3] = crc;

        return 4;
    }
};

struct Entangle : public BaseC
{
    Entangle(uint8_t fidelity, uint8_t nghbr, uint8_t qid) : f(fidelity), n(nghbr), q(qid) {}
    uint8_t f;
    uint8_t n;
    uint8_t q;

    std::size_t create_command(uint8_t *buff, std::size_t max_len) const
    {

        // payload size is fixed: 4 bytes
        if (max_len < 4 || buff == nullptr)
            return 0;

        buff[0] = Command_t::ENTANGLEMENT_ATTEMPT;
        buff[1] = ((n << 4) & NGHBR_BIT_MASK) | (FIDELITY_BIT_MASK & f);
        buff[2] = (q << 2) & QID_BIT_MASK;
        uint8_t crc = crc8_ccitt(buff, 3);
        buff[3] = crc;

        return 4;
    }
};

struct EntanMeasure : public BaseC
{
    EntanMeasure(uint8_t fidelity, uint8_t nghbr, uint8_t qid, bool try_now, bool basis, uint8_t repeat) : f(fidelity), n(nghbr), q(qid), now(try_now), b(basis), r(repeat) {}
    uint8_t f;
    uint8_t n;
    uint8_t q;
    bool now;
    bool b;
    uint8_t r;

    std::size_t create_command(uint8_t *buff, std::size_t max_len) const override
    {

        // payload size is fixed: 4 bytes
        if (max_len < 4 || buff == nullptr)
            return 0;

        buff[0] = Command_t::ENTANGLEMENT_MEASURE;
        buff[1] = ((n << 4) & NGHBR_BIT_MASK) | (FIDELITY_BIT_MASK & f);
        buff[2] = ((r << 4) & 0xf0) | ((q << 2) & QID_BIT_MASK) | ((now & 0x01) << 1) | (b & BASIS_BIT_MASK);
        uint8_t crc = crc8_ccitt(buff, 3);
        buff[3] = crc;

        return 4;
    }
};

struct PreMeasureGateSelect : public BaseC
{
    PreMeasureGateSelect(Angle_t angle1, Angle_t angle2, Angle_t angle3, bool target) : a1(angle1), a2(angle2), a3(angle3), t(target) {}
    Angle_t a1;
    Angle_t a2;
    Angle_t a3;
    bool t;

    std::size_t create_command(uint8_t *buff, std::size_t max_len) const
    {

        // payload size is fixed: 4 bytes
        if (max_len < 4 || buff == nullptr)
            return 0;

        buff[0] = Command_t::PRE_MEASUREMENT_GATES_SELECTION;
        buff[1] = ((a2 & 0x0e) << 5) | (ANGLE_BIT_MASK & a1);
        buff[2] = ((t << 7) & 0x80) | ((a3 << 2) & 0xec) | ((a2 & 0x18) >> 3);
        uint8_t crc = crc8_ccitt(buff, 3);
        buff[3] = crc;

        return 4;
    }
};

struct QSwap : public BaseC
{
    QSwap(uint8_t q1_addr, uint8_t q2_addr) : addr1(q1_addr), addr2(q2_addr) {}
    uint8_t addr1;
    uint8_t addr2;

    std::size_t create_command(uint8_t *buff, std::size_t max_len) const
    {

        // payload size is fixed: 4 bytes
        if (max_len < 4 || buff == nullptr)
            return 0;

        buff[0] = Command_t::QUBIT_SWAP;
        buff[1] = (QUBIT_1_BIT_MASK & addr1) | ((addr2 << 4) & QUBIT_2_BIT_MASK);
        buff[2] = 0x00;
        uint8_t crc = crc8_ccitt(buff, 3);
        buff[3] = crc;

        return 4;
    }
};

struct QMove : public BaseC
{
    QMove(uint8_t q1_addr, uint8_t q2_addr) : addr1(q1_addr), addr2(q2_addr) {}
    uint8_t addr1;
    uint8_t addr2;

    std::size_t create_command(uint8_t *buff, std::size_t max_len) const
    {

        // payload size is fixed: 4 bytes
        if (max_len < 4 || buff == nullptr)
            return 0;

        buff[0] = Command_t::QUBIT_MOVE;
        buff[1] = (QUBIT_1_BIT_MASK & addr1) | ((addr2 << 4) & QUBIT_2_BIT_MASK);
        buff[2] = 0x00;
        uint8_t crc = crc8_ccitt(buff, 3);
        buff[3] = crc;

        return 4;
    }
};

struct EntangleSwap : public BaseC
{
    EntangleSwap(uint8_t q1_addr, uint8_t q2_addr) : addr1(q1_addr), addr2(q2_addr) {}
    uint8_t addr1;
    uint8_t addr2;

    std::size_t create_command(uint8_t *buff, std::size_t max_len) const
    {

        // payload size is fixed: 4 bytes
        if (max_len < 4 || buff == nullptr)
            return 0;

        buff[0] = Command_t::ENTANGLEMENT_SWAP;
        buff[1] = (QUBIT_1_BIT_MASK & addr1) | ((addr2 << 4) & QUBIT_2_BIT_MASK);
        buff[2] = 0x00;
        uint8_t crc = crc8_ccitt(buff, 3);
        buff[3] = crc;

        return 4;
    }
};