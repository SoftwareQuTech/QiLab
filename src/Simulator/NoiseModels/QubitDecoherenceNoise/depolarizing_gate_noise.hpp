// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

/*
** Created on: 02.07.2026
** Created by: Claude
** Description: Contains the depolarizing gate noise models for the simulator.
**              See QILab_Noise_Models.pdf Sec. 2: gate noise is a depolarizing
**              channel applied per gate instruction. The models only own the
**              depolarizing probability p; the Pauli sampling itself is done
**              by STIM's built-in DEPOLARIZE1 / DEPOLARIZE2 channels.
**
**              Granularity (Sec. 2.1):
**              - Single-qubit gate noise: one model instance per qubit array.
**              - Two-qubit gate noise: one model instance per node.
**
**              Out-of-range parameters are rejected, not repaired: the model
**              is marked invalid (is_valid() == false), an error is logged and
**              the model behaves as noiseless (p = 0).
*/

#pragma once
#include <string>

#include "logger/logger_api.hpp"

// Config for both gate depolarizing models: a single raw probability p. The
// same block shape is used per qubit array (single-qubit gates) and per node
// (two-qubit gates); the two differ only in the maximum p each model accepts,
// which the model constructors enforce.
struct GateNoiseDepolarizingConfig
{
    float depolarizing_probability;
};

// Single-qubit depolarizing gate noise (one instance per qubit array).
// Channel: E(rho) = (1 - p) rho + p/3 (X rho X + Y rho Y + Z rho Z)
// Fidelity relation (Eq. 2): F = 1 - 2p/3  <=>  p = 3/2 (1 - F)
class SingleQubitGateNoiseModel
{
public:
    // Maximal mixing for the single-qubit depolarizing channel.
    static constexpr float MAX_DEPOLARIZING_PROBABILITY = 0.75f;

    explicit SingleQubitGateNoiseModel(float depolarizing_probability)
    {
        if (depolarizing_probability < 0.0f || depolarizing_probability > MAX_DEPOLARIZING_PROBABILITY)
        {
            LOG_ERROR("Invalid single-qubit gate depolarizing probability " +
                      std::to_string(depolarizing_probability) + ". Must be in [0, " +
                      std::to_string(MAX_DEPOLARIZING_PROBABILITY) + "]. Model marked invalid.");
            valid = false;
            return;
        }
        p = depolarizing_probability;
    }

    // Convert a target gate fidelity to the depolarizing probability p (Eq. 2).
    // Returns -1.0f if the fidelity is outside [0.5, 1] (below maximal mixing).
    static float p_from_fidelity(float fidelity)
    {
        if (fidelity < 0.5f || fidelity > 1.0f)
        {
            LOG_ERROR("Invalid single-qubit gate fidelity " + std::to_string(fidelity) +
                      ". Must be in [0.5, 1].");
            return -1.0f;
        }
        return 1.5f * (1.0f - fidelity);
    }

    bool is_valid() const
    {
        return valid;
    }

    // Probability p to feed to STIM's DEPOLARIZE1 channel.
    float get_depolarizing_probability() const
    {
        return p;
    }

private:
    float p = 0.0f;
    bool valid = true;
};

// Two-qubit depolarizing gate noise (one instance per node).
// Channel: E(rho) = (1 - p) rho + p/15 * sum over the 15 non-identity Pauli pairs (Eq. 4)
// Fidelity relation (Eq. 5): F = 1 - 4p/5  <=>  p = 5/4 (1 - F)
class TwoQubitGateNoiseModel
{
public:
    // Maximal mixing for the two-qubit depolarizing channel (93.75%).
    static constexpr float MAX_DEPOLARIZING_PROBABILITY = 0.9375f;

    explicit TwoQubitGateNoiseModel(float depolarizing_probability)
    {
        if (depolarizing_probability < 0.0f || depolarizing_probability > MAX_DEPOLARIZING_PROBABILITY)
        {
            LOG_ERROR("Invalid two-qubit gate depolarizing probability " +
                      std::to_string(depolarizing_probability) + ". Must be in [0, " +
                      std::to_string(MAX_DEPOLARIZING_PROBABILITY) + "]. Model marked invalid.");
            valid = false;
            return;
        }
        p = depolarizing_probability;
    }

    // Convert a target gate fidelity to the depolarizing probability p (Eq. 5).
    // Returns -1.0f if the fidelity is outside [0.25, 1] (below maximal mixing).
    static float p_from_fidelity(float fidelity)
    {
        if (fidelity < 0.25f || fidelity > 1.0f)
        {
            LOG_ERROR("Invalid two-qubit gate fidelity " + std::to_string(fidelity) +
                      ". Must be in [0.25, 1].");
            return -1.0f;
        }
        return 1.25f * (1.0f - fidelity);
    }

    bool is_valid() const
    {
        return valid;
    }

    // Probability p to feed to STIM's DEPOLARIZE2 channel.
    float get_depolarizing_probability() const
    {
        return p;
    }

private:
    float p = 0.0f;
    bool valid = true;
};
