/*
** Created on: 22.08.2025
** Created by: mrkolev
** Description: Contains a mock adapter in order to do initial testing of adapter timings
*/
#pragma once
#include "Simulator/sim_interface.hpp"
#include "logger/logger.hpp"

class MockAdapter : ISimulator
{
    public:
    //MockAdapter(const ILogger&);
    MockAdapter();
    ~MockAdapter();
    bool qubit_initialize(uint8_t qubit_address);
    bool single_qubit_gate(uint8_t qubit_address, Angle_t angle, Axis_t axis);
    bool two_qubit_gate(uint8_t qubit1_address, uint8_t qubit2_address, Angle_t angle, Axis_t axis);
    void qubit_measure(uint8_t qubit_address, bool basis);
    void attempt_entanglement(uint16_t fidelity, uint8_t neighbour_address, uint8_t qid);
    void entanglement_measure(uint16_t fidelity, uint8_t neighbour_address, bool basis, uint8_t qid, bool try_now, uint8_t repetitions);
    void pre_measure_gates_selection(bool target, Angle_t angle_1, Angle_t angle_2, Angle_t angle_3);
    void qubit_move(uint8_t qubit1_address, uint8_t qubit2_address);
    void qubit_swap(uint8_t qubit1_address, uint8_t qubit2_address);
    void entanglement_swap(uint8_t qubit1_address, uint8_t qubit2_address);

    private:
    //have a reference to the logger used for performance estimations
    //ILogger* local_logger;

    MockAdapter(const MockAdapter&) = delete;
    MockAdapter(const MockAdapter&&) = delete;
    MockAdapter& operator=(MockAdapter&) = delete;
    MockAdapter& operator=(MockAdapter&&) = delete;
};