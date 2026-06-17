/*
** Created on: 20.04.2026
** Created by: mrkolev
** Description: Contains the interface to possible entaglement noise models for the simulator
*/

#pragma once
#include <cstdint>  

struct EntanglementNoiseInterface
{
    virtual ~EntanglementNoiseInterface() = default;
    virtual float calculate_entanglement_success_probability() = 0; // Pure virtual function to calculate the success probability of entanglement
    virtual float calculate_entanglement_fidelity() = 0; // Pure virtual function to calculate the fidelity of the entangled state
};