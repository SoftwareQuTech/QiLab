/*
** Created on: 22.08.2025
** Created by: mrkolev
** Description: Contains a mock adapter in order to do initial testing of adapter timings
*/
#pragma once
#include "Simulator/sim_interface.hpp"
#include "logger/logger.hpp"

class StimAdapter : ISimulator
{
    public:
    //MockAdapter(const ILogger&);
    StimAdapter();
    ~StimAdapter();
    bool qubit_initialize(uint8_t qubit_address) override;
    bool single_qubit_gate(uint8_t qubit_address, Angle_t angle, Axis_t axis) override;
    bool two_qubit_gate(uint8_t qubit1_address, uint8_t qubit2_address, Angle_t angle, Axis_t axis) override;
    uint8_t qubit_measure(uint8_t qubit_address, bool basis) override;
    uint8_t attempt_entanglement(uint16_t fidelity, uint8_t neighbour_address, uint8_t qid);
    uint8_t entanglement_measure(uint16_t fidelity, uint8_t neighbour_address, bool basis, uint8_t qid, bool try_now, uint8_t repetitions) override;
    void pre_measure_gates_selection(bool target, Angle_t angle_1, Angle_t angle_2, Angle_t angle_3) override;
    void qubit_move(uint8_t qubit1_address, uint8_t qubit2_address) override;
    void qubit_swap(uint8_t qubit1_address, uint8_t qubit2_address) override;
    uint8_t entanglement_swap(uint8_t qubit1_address, uint8_t qubit2_address) override;

    private:
    //have a reference to the logger used for performance estimations
    //ILogger* local_logger;

    StimAdapter(const StimAdapter&) = delete;
    StimAdapter(const StimAdapter&&) = delete;
    StimAdapter& operator=(StimAdapter&) = delete;
    StimAdapter& operator=(StimAdapter&&) = delete;
};