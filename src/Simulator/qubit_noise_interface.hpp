// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

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
    /// @brief Mark the qubit as touched by an operation, updating its internal state accordingly.
    /// @param delay The delay in nanoseconds to apply before marking the qubit as touched, this is needed, when a 
    ///             specific gate has a long duration, and the noise should be updated after the delay has passed, and not before, which would be unrealistic
    virtual int mark_touched(uint64_t delay = 0) = 0; // Pure virtual function to mark the qubit as touched
    virtual float get_qubit_decoherence_probability() = 0; // Pure virtual function to get the decoherence probability of the qubit
};