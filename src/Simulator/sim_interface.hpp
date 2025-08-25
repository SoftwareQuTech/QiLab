/*
** Created on: 21.08.2025
** Created by: mrkolev
** Description: Contains the interface to possible simulator adapters
*/

#pragma once
#include <cstdint>

#include "DataTypes/commands.hpp"

struct ISimulator
{
    virtual ~ISimulator() = default;
    
    virtual bool qubit_initialize(uint8_t qubit_address) = 0;
    virtual bool single_qubit_gate(uint8_t qubit_address, Angle_t angle, Axis_t axis) = 0;
    virtual bool two_qubit_gate(uint8_t qubit1_address, uint8_t qubit2_address, Angle_t angle, Axis_t axis) = 0;
    virtual uint8_t qubit_measure(uint8_t qubit_address, bool basis) = 0;
    virtual uint8_t attempt_entanglement(uint16_t fidelity, uint8_t neighbour_address, uint8_t qid) = 0;
    virtual uint8_t entanglement_measure(uint16_t fidelity, uint8_t neighbour_address, bool basis, uint8_t qid, bool try_now, uint8_t repetitions) = 0;
    virtual void pre_measure_gates_selection(bool target, Angle_t angle_1, Angle_t angle_2, Angle_t angle_3) = 0;
    virtual void qubit_move(uint8_t qubit1_address, uint8_t qubit2_address) = 0;
    virtual void qubit_swap(uint8_t qubit1_address, uint8_t qubit2_address) = 0;
    virtual uint8_t entanglement_swap(uint8_t qubit1_address, uint8_t qubit2_address) = 0;
};