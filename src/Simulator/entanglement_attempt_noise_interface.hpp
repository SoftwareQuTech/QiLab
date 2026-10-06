// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

/*
** Created on: 22.07.2026
** Created by: Claude
** Description: Contains the interface to entanglement-attempt models
**              (QILab_Noise_Models.pdf Sec. 4.2) for the entanglement
**              manager. The model is attached per link (pair of nodes) and
**              covers the duration and success probability of batched
**              entanglement-generation attempts.
*/

#pragma once
#include <cstdint>

// Outcome of one batch of entanglement attempts (Sec. 4.2).
// On success: attempts = k (the successful attempt), duration_ns = k * t_attempt.
// On failure: attempts = batch_size (the whole batch was consumed),
// duration_ns = batch_size * t_attempt. The caller resubmits by calling
// attempt_batch() again; per Eqs. 8-10 repeated batches compose to exactly the
// geometric distribution of Eq. 6.
struct BatchOutcome
{
    bool success;
    uint32_t attempts;
    uint64_t duration_ns;
};

struct EntanglementAttemptNoiseInterface
{
    virtual ~EntanglementAttemptNoiseInterface() = default;
    virtual BatchOutcome attempt_batch() = 0; // Run one batch of <= batch_size entanglement attempts
};
