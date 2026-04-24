/*
** Created on: 20.04.2026
** Created by: mrkolev
** Description: Contains the interface to possible qubit noise models for the simulator
*/

#pragma once
#include <cstdint>  

struct QubitNoiseInterface
{
    virtual ~QubitNoiseInterface() = default;
    virtual int mark_touched() = 0; // Pure virtual function to mark the qubit as touched
    virtual float get_qubit_decoherence_probability() = 0; // Pure virtual function to get the decoherence probability of the qubit
};