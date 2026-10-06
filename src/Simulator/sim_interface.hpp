// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

/*
** Created on: 21.08.2025
** Created by: mrkolev
** Description: Contains the interface to possible simulator adapters
*/

#pragma once
#include <cstdint>

#include "DataTypes/commands.hpp"

/**
 * @file
 * @brief Public interface for pluggable quantum simulator backends.
 *
 * The ISimulator abstraction defines the minimal set of operations required by
 * the higher-level runtime to interact with a quantum simulator or hardware
 * adapter. Implementations may target different engines (CPU/GPU simulators,
 * hardware drivers, remote services) but must preserve the semantics described
 * here.
 *
 * @note Parameter types such as ::Angle_t and ::Axis_t are defined in
 *       @c DataTypes/commands.hpp. Implementations should document the accepted
 *       units/ranges (e.g., radians vs degrees) if not implied by those types.
 *
 * @par Thread-safety
 * Unless stated otherwise by a concrete implementation, instances are not
 * guaranteed to be thread-safe. If you need concurrent access, guard calls
 * externally or provide a thread-safe implementation.
 */
struct ISimulator
{
    /// Virtual destructor to allow proper cleanup via base pointer.
    virtual ~ISimulator() = default;

    /**
     * @brief Initialize (reset/prepare) a single qubit to the default basis state.
     *
     * Typical behavior is to prepare at @p qubit_address, clearing any
     * prior state, pending operations, or entanglement metadata.
     *
     * @param qubit_address Logical/physical address of the target qubit.
     * @return true on success; false if the address is invalid or initialization failed.
     *
     * @note Implementations should specify whether this operation breaks existing
     *       entanglement involving the target qubit.
     */
    virtual bool qubit_initialize(uint8_t qubit_address) = 0;

    /**
     * @brief Apply a single-qubit rotation or gate on the specified axis.
     *
     * Applies the gate described by @p axis and @p angle to @p qubit_address.
     * The exact mapping from (axis, angle) to a unitary (e.g., RX/RY/RZ or named
     * gates) is defined by the implementation and/or the ::Axis_t documentation.
     *
     * @param qubit_address Target qubit address.
     * @param angle Rotation angle or parameter for the gate (unit as defined by ::Angle_t).
     * @param axis Axis or gate selector (as defined by ::Axis_t).
     * @return true on success; false on invalid parameters or unsupported gate.
     */
    virtual bool single_qubit_gate(uint8_t qubit_address,
                                   Angle_t angle,
                                   Axis_t axis) = 0;

    /**
     * @brief Apply a single-qubit noise channel to the specified qubit.
     *     * Applies a Pauli noise channel to @p qubit_address with the specified
     * probabilities for X, Y, and Z errors. The sum of the probabilities should
     * not exceed 1.0. The implementation uses the underlying simulator's noise
     * functionality to apply the noise.
     *     * @param qubit_address Target qubit address.
     * @param x_noise_probability Probability of an X error occurring on the qubit.
     * @param y_noise_probability Probability of a Y error occurring on the qubit.
     * @param z_noise_probability Probability of a Z error occurring on the qubit.
     * @return true on success; false on invalid parameters or unsupported noise channel.
     */
    virtual bool single_qubit_noise(uint8_t qubit_address,
                                   float x_noise_probability,
                                   float y_noise_probability,
                                   float z_noise_probability) = 0;

    /**
     * @brief Apply a two-qubit depolarizing channel to a qubit pair.
     *
     * Applies E(rho) = (1-p) rho + p/15 * sum over the 15 non-identity two-qubit
     * Pauli pairs (QILab_Noise_Models.pdf Sec. 2 Eq. 4). This is a CORRELATED
     * channel: it is deliberately not expressible as two single_qubit_noise
     * calls, nor through compose_pauli_channels() (a single-qubit composer),
     * which is why it needs its own adapter entry point.
     *
     * Unlike single_qubit_noise this takes one scalar p rather than per-Pauli
     * weights: no configured model produces a general 15-weight two-qubit Pauli
     * channel, so the interface stays at the depolarizing special case.
     *
     * @param qubit1_address First qubit address.
     * @param qubit2_address Second qubit address; must differ from the first.
     * @param depolarizing_probability p in [0, 0.9375]; p == 0 is a no-op.
     * @return true on success; false on invalid parameters or addresses.
     */
    virtual bool two_qubit_depolarizing_noise(uint8_t qubit1_address,
                                              uint8_t qubit2_address,
                                              float depolarizing_probability) = 0;


    /**
     * @brief Apply a two-qubit gate between a control/target pair.
     *
     * Applies the two-qubit operation selected by @p axis and parameterized by
     * @p angle to (@p qubit1_address, @p qubit2_address).
     *
     * @param qubit1_address Address of the first qubit (role is implementation-defined).
     * @param qubit2_address Address of the second qubit.
     * @param angle Optional parameter for parameterized two-qubit gates.
     * @param axis  Selector indicating the specific two-qubit gate.
     * @return true on success; false if unsupported or addresses are invalid.
     *
     * @note If control/target roles matter, document the convention used
     *       (e.g., @p qubit1_address = control for CX).
     */
    virtual bool two_qubit_gate(uint8_t qubit1_address,
                                uint8_t qubit2_address,
                                Angle_t angle,
                                Axis_t axis) = 0;

    /**
     * @brief Perform a projective measurement on a single qubit.
     *
     * Measures @p qubit_address in the basis indicated by @p basis and collapses
     * the state accordingly.
     *
     * @param qubit_address Target qubit to measure.
     * @param basis Basis selector; semantics depend on your command types
     *              (e.g., false = Z-basis, true = X-basis).
     * @return The classical outcome bit (0 or 1).
     *
     * @note Implementations should specify whether measurement is destructive
     *       (state reset) or leaves post-measurement state in the measured eigenstate.
     */
    virtual uint8_t qubit_measure(uint8_t qubit_address, bool basis) = 0;

    /**
     * @brief Attempt to create entanglement with a neighbor/link.
     *
     * Tries to establish an entangled pair with @p neighbour_address using the
     * provided @p fidelity target and identifies the local qubit by @p qid.
     *
     * @param fidelity Desired/target fidelity for the entanglement attempt.
     * @param neighbour_address Address/identifier of the neighbor/link.
     * @param local_qubit Local qubit identifier used for the entanglement context.
     * @param is_initiator Whether this node is the initiator (applies gates) or responder (just measures).
     * @param qid Local qubit identifier used for the entanglement context.
     * @return Implementation-defined status/result code.
     *
     * @retval 0..255 Meaning must be documented by the implementation (e.g., 0 = success).
     */
    virtual int8_t attempt_entanglement(uint16_t fidelity,
                                         uint8_t local_qubit,
                                         uint8_t neighbour_address,
                                         bool is_initiator,
                                         uint8_t qid) = 0;

    /**
     * @note Entanglement measurement is orchestrated by the SimulatorManager, not
     *       the adapter: it establishes/identifies the pair via
     *       @c attempt_entanglement, applies any basis-rotation gates via
     *       @c single_qubit_gate, then reads the outcome via @c qubit_measure.
     *       There is deliberately no @c entanglement_measure or
     *       @c pre_measure_gates_selection on this interface -- the adapter holds
     *       no per-node pre-measurement state.
     */

    /**
     * @brief Move (teleport/route/alias) a qubit state from one address to another.
     *
     * Reassigns or transports the logical state at @p qubit1_address to
     * @p qubit2_address. Semantics depend on the backend (e.g., remapping a
     * register index vs. performing a physical move/teleport step).
     *
     * @param qubit1_address Source qubit address.
     * @param qubit2_address Destination qubit address.
     */
    virtual bool qubit_move(uint8_t qubit1_address, uint8_t qubit2_address) = 0;

    /**
     * @brief Swap the quantum states of two qubits.
     *
     * Performs a logical SWAP between the states at @p qubit1_address and
     * @p qubit2_address 
     *
     * @param qubit1_address First qubit address.
     * @param qubit2_address Second qubit address.
     */
    virtual bool qubit_swap(uint8_t qubit1_address, uint8_t qubit2_address) = 0;

    /**
     * @brief Perform entanglement swapping between two local qubits.
     *
     * Consumes two entangled links (each involving one of the given qubits)
     * to extend entanglement across a longer path, returning a status/result code.
     *
     * @param qubit1_address Address of the first local qubit participating in the swap.
     * @param qubit2_address Address of the second local qubit.
     * @return Implementation-defined status/result code (e.g., success/failure).
     *
     */
    virtual bool entanglement_swap(uint8_t qubit1_address,
                                      uint8_t qubit2_address) = 0;

    virtual bool allocate_qubits(uint8_t num_qubits) = 0;

};
