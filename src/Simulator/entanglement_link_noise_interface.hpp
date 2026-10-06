// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

/*
** Created on: 22.07.2026
** Created by: Claude
** Description: Contains the interface to entanglement-link noise models
**              (QILab_Noise_Models.pdf Sec. 4.3) for the simulator manager.
**              The model is attached per link (pair of nodes) and covers the
**              Pauli noise channel applied to the qubit pair once an
**              entanglement attempt succeeds.
*/

#pragma once
#include "Simulator/pauli_channel_math.hpp"

// PauliChannelWeights (feeds STIM's PAULI_CHANNEL_1 with q_x, q_y, q_z; q_i is
// the inferred identity weight) is defined in pauli_channel_math.hpp, shared
// with single-qubit gate noise composition.
// NOTE: this is a SINGLE-qubit channel - it is applied to only one of the two
// qubits forming the entangled pair (the initiator, i.e. the lower node id).

struct EntanglementLinkNoiseInterface
{
    virtual ~EntanglementLinkNoiseInterface() = default;
    virtual PauliChannelWeights get_pauli_channel_weights() = 0; // Combined dephasing+depolarizing channel (Eq. 17)
};
