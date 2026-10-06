// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

/*
** Created on: 22.08.2025
** Created by: mrkolev
** Description: Contains a mock adapter in order to do initial testing of adapter timings
*/
#pragma once
#include "Simulator/sim_interface.hpp"
#include "logger/logger.hpp"

class MockAdapter final : public ISimulator
{
    public:
    //MockAdapter(const ILogger&);
    MockAdapter();
    ~MockAdapter();
    bool qubit_initialize(uint8_t qubit_address) ;
    bool single_qubit_gate(uint8_t qubit_address, Angle_t angle, Axis_t axis) ;
    bool single_qubit_noise(uint8_t qubit_address, float x_noise_probability, float y_noise_probability, float z_noise_probability) ;
    bool two_qubit_depolarizing_noise(uint8_t qubit1_address, uint8_t qubit2_address, float depolarizing_probability) ;
    bool two_qubit_gate(uint8_t qubit1_address, uint8_t qubit2_address, Angle_t angle, Axis_t axis) ;
    uint8_t qubit_measure(uint8_t qubit_address, bool basis) ;
    int8_t attempt_entanglement(uint16_t fidelity, uint8_t local_qubit, uint8_t neighbour_address, bool is_initiator, uint8_t qid) ;
    int8_t entanglement_measure(uint16_t fidelity, uint8_t neighbour_address, bool basis, bool is_initiator, uint8_t qid, bool try_now, uint8_t repetitions) ;
    void pre_measure_gates_selection(bool target, Angle_t angle_1, Angle_t angle_2, Angle_t angle_3) ;
    bool qubit_move(uint8_t qubit1_address, uint8_t qubit2_address) ;
    bool qubit_swap(uint8_t qubit1_address, uint8_t qubit2_address) ;
    bool entanglement_swap(uint8_t qubit1_address, uint8_t qubit2_address) ;
    bool allocate_qubits(uint8_t num_qubits) ;
    bool apply_qubit_noise(uint8_t qubit_address, float p_dephasing, float p_depolarizing);
    private:
    //have a reference to the logger used for performance estimations
    //ILogger* local_logger;

    MockAdapter(const MockAdapter&) = delete;
    MockAdapter(const MockAdapter&&) = delete;
    MockAdapter& operator=(MockAdapter&) = delete;
    MockAdapter& operator=(MockAdapter&&) = delete;
};