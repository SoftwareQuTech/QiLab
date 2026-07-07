/*
** Created on: 02.07.2026
** Created by: Claude
** Description: Contains the interface to entanglement-generation noise models
**              for the simulator (QILab_Noise_Models.pdf Sec. 4). The model is
**              attached per link (pair of nodes) and covers both the duration
**              of entanglement generation (batched geometric attempts) and the
**              noise channel applied to the generated entangled state.
*/

#pragma once
#include <cstdint>

// Weights of the combined dephasing + depolarizing channel expressed as a
// single-qubit Pauli channel (Eq. 16/17). Feeds STIM's PAULI_CHANNEL_1 with
// (q_x, q_y, q_z); q_i is the inferred identity weight.
// NOTE: this is a SINGLE-qubit channel - it is applied to only one of the two
// qubits forming the entangled pair (e.g., always the node with lower id).
struct PauliChannelWeights
{
    float q_i;
    float q_x;
    float q_y;
    float q_z;
};

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

struct EntanglementGenerationNoiseInterface
{
    virtual ~EntanglementGenerationNoiseInterface() = default;
    virtual BatchOutcome attempt_batch() = 0; // Run one batch of <= batch_size entanglement attempts
    virtual PauliChannelWeights get_pauli_channel_weights() = 0; // Combined dephasing+depolarizing channel (Eq. 17)
};
