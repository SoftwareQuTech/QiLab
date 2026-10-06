// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

/*
** Created on: 21.07.2026
** Created by: mrkolev
** Description: Shared, backend-agnostic single-qubit Pauli channel math. Used
**              both to compose gate noise (idle dephasing + gate depolarizing)
**              and entanglement-generation noise (link dephasing + link
**              depolarizing) -- the composition formula is identical in both
**              cases, only the two input channels differ.
*/

#pragma once
#include "logger/logger_api.hpp"

// Weights of a combined single-qubit Pauli channel; q_i is the identity weight.
struct PauliChannelWeights
{
    float q_i;
    float q_x;
    float q_y;
    float q_z;
};

// Compose two independent single-qubit Pauli channels -- channel 1
// (x1, y1, z1) and channel 2 (x2, y2, z2) -- into the single equivalent
// channel that results from applying one after the other.
inline PauliChannelWeights compose_pauli_channels(float x1_noise_probability, float y1_noise_probability, float z1_noise_probability,
                                                  float x2_noise_probability, float y2_noise_probability, float z2_noise_probability)
{
    PauliChannelWeights composed_weights{};

    float identity1_probability = 1.0f - (x1_noise_probability + y1_noise_probability + z1_noise_probability);
    float identity2_probability = 1.0f - (x2_noise_probability + y2_noise_probability + z2_noise_probability);

    composed_weights.q_x = identity1_probability * x2_noise_probability + x1_noise_probability * identity2_probability + y1_noise_probability * z2_noise_probability + z1_noise_probability * y2_noise_probability;
    composed_weights.q_y = identity1_probability * y2_noise_probability + y1_noise_probability * identity2_probability + x1_noise_probability * z2_noise_probability + z1_noise_probability * x2_noise_probability;
    composed_weights.q_z = identity1_probability * z2_noise_probability + z1_noise_probability * identity2_probability + x1_noise_probability * y2_noise_probability + y1_noise_probability * x2_noise_probability;

    if (composed_weights.q_x + composed_weights.q_y + composed_weights.q_z > 1.0f)
    {
        LOG_WARNING("Composed single-qubit noise channel has invalid probabilities.");
        return PauliChannelWeights{1.0f, 0.0f, 0.0f, 0.0f}; // Identity channel on error
    }

    composed_weights.q_i = 1.0f - (composed_weights.q_x + composed_weights.q_y + composed_weights.q_z);
    return composed_weights;
}
