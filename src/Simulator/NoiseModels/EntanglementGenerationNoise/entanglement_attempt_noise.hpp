// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

/*
** Created on: 22.07.2026
** Created by: Claude
** Description: Contains the entanglement-attempt model for the entanglement
**              manager (QILab_Noise_Models.pdf Sec. 4.2). One instance per
**              link (pair of nodes).
**
**              Duration: generation is a sequence of independent attempts,
**              each succeeding with probability p_success (Eq. 6). Attempts
**              are capped per batch by batch_size; a failed batch is
**              reported to the caller.
**
**              Out-of-range parameters are rejected, not repaired: the model
**              is marked invalid (is_valid() == false), an error is logged,
**              attempt_batch() always fails.
*/

#pragma once
#include <cstdint>
#include <random>
#include <string>

#include "Simulator/entanglement_attempt_noise_interface.hpp"
#include "logger/logger_api.hpp"

struct EntanglementAttemptBasicConfig
{
    uint64_t attempt_duration_ns;
    uint32_t batch_size;
    float p_success;
    uint64_t rng_seed;
};

class EntanglementAttemptModel : public EntanglementAttemptNoiseInterface
{
public:
    EntanglementAttemptModel(uint64_t attempt_duration_ns,
                             uint32_t batch_size,
                             float p_success,
                             uint64_t rng_seed)
        : attempt_duration_ns(attempt_duration_ns),
          batch_size(batch_size),
          p_success(p_success),
          rng(rng_seed)
    {
        if (attempt_duration_ns == 0)
        {
            LOG_ERROR("Invalid entanglement attempt duration 0 ns. Must be > 0. Model marked invalid.");
            valid = false;
        }
        if (batch_size == 0)
        {
            LOG_ERROR("Invalid entanglement batch size 0. Must be > 0. Model marked invalid.");
            valid = false;
        }
        if (p_success < 0.0f || p_success > 1.0f)
        {
            LOG_ERROR("Invalid entanglement success probability " + std::to_string(p_success) +
                      ". Must be in [0, 1]. Model marked invalid.");
            valid = false;
        }
    }

    ~EntanglementAttemptModel() override = default;

    bool is_valid() const
    {
        return valid;
    }

    // Run one batch of <= batch_size attempts. Samples the number of attempts
    // until success from the geometric distribution Pr(k) = p (1-p)^(k-1)
    // (Eq. 6). Sampling once and capping by the batch size is probabilistically
    // identical to attempt-by-attempt batching (Eqs. 8-10).
    BatchOutcome attempt_batch() override
    {
        if (!valid)
        {
            return BatchOutcome{false, 0, 0};
        }

        if (p_success <= 0.0f)
        {
            return BatchOutcome{false, batch_size, batch_size * attempt_duration_ns};
        }

        if (p_success >= 1.0f)
        {
            return BatchOutcome{true, 1, attempt_duration_ns};
        }

        // std::geometric_distribution returns the number of failures before the
        // first success, so the successful attempt is k = failures + 1.
        std::geometric_distribution<uint64_t> attempts_distribution(p_success);
        const uint64_t k = attempts_distribution(rng) + 1;
        LOG_INFO("Entanglement attempt batch: sampled " + std::to_string(k) + " attempts until success (p_success = " +
                 std::to_string(p_success) + ", batch_size = " + std::to_string(batch_size) + ").");
        if (k > batch_size)
        {
            return BatchOutcome{false, batch_size, batch_size * attempt_duration_ns};
        }

        const auto attempts = static_cast<uint32_t>(k);
        return BatchOutcome{true, attempts, attempts * attempt_duration_ns};
    }

private:
    uint64_t attempt_duration_ns;
    uint32_t batch_size;
    float p_success;
    std::mt19937 rng;
    bool valid = true;
};
