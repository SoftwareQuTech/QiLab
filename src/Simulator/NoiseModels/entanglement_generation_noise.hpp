/*
** Created on: 02.07.2026
** Created by: Claude
** Description: Contains the entanglement-generation noise model for the
**              simulator (QILab_Noise_Models.pdf Sec. 4). One instance per
**              link (pair of nodes).
**
**              Duration (Sec. 4.2): generation is a sequence of independent
**              attempts, each succeeding with probability p_success (Eq. 6).
**              Attempts are capped per batch by batch_size; a failed batch is
**              reported to the caller, which resubmits (QNodeOS behavior).
**
**              Noise channel (Sec. 4.3): dephasing (p_z) composed with
**              depolarizing (p_d), collapsed into a single biased single-qubit
**              Pauli channel (Eq. 17) applied to one qubit of the pair.
**
**              Out-of-range parameters are rejected, not repaired: the model
**              is marked invalid (is_valid() == false), an error is logged,
**              attempt_batch() always fails and the channel is the identity.
*/

#pragma once
#include <cstdint>
#include <random>
#include <string>

#include "Simulator/entanglement_generation_noise_interface.hpp"
#include "logger/logger_api.hpp"

class EntanglementGenerationModel : public EntanglementGenerationNoiseInterface
{
public:
    EntanglementGenerationModel(uint64_t attempt_duration_ns,
                                uint32_t batch_size,
                                float p_success,
                                float p_depolarizing,
                                float p_dephasing,
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
        if (p_depolarizing < 0.0f || p_depolarizing > 1.0f)
        {
            LOG_ERROR("Invalid entanglement depolarizing probability " + std::to_string(p_depolarizing) +
                      ". Must be in [0, 1]. Model marked invalid.");
            valid = false;
        }
        if (p_dephasing < 0.0f || p_dephasing > 1.0f)
        {
            LOG_ERROR("Invalid entanglement dephasing probability " + std::to_string(p_dephasing) +
                      ". Must be in [0, 1]. Model marked invalid.");
            valid = false;
        }

        if (!valid)
        {
            return; // Weights stay at the identity channel
        }

        // Combined dephasing-then-depolarizing channel as one Pauli channel (Eq. 17).
        // X and Y retain the depolarizing weight p_d/3, Z carries an additional
        // share contributed by the dephasing step.
        weights.q_x = p_depolarizing / 3.0f;
        weights.q_y = p_depolarizing / 3.0f;
        weights.q_z = p_dephasing * (1.0f - p_depolarizing) + (p_depolarizing / 3.0f) * (1.0f - p_dephasing);
        weights.q_i = (1.0f - p_dephasing) * (1.0f - p_depolarizing) + (p_depolarizing / 3.0f) * p_dephasing;
    }

    ~EntanglementGenerationModel() override = default;

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

        if (k > batch_size)
        {
            return BatchOutcome{false, batch_size, batch_size * attempt_duration_ns};
        }

        const auto attempts = static_cast<uint32_t>(k);
        return BatchOutcome{true, attempts, attempts * attempt_duration_ns};
    }

    PauliChannelWeights get_pauli_channel_weights() override
    {
        return weights;
    }

private:
    uint64_t attempt_duration_ns;
    uint32_t batch_size;
    float p_success;
    PauliChannelWeights weights{1.0f, 0.0f, 0.0f, 0.0f}; // Identity channel until validated
    std::mt19937 rng;
    bool valid = true;
};
