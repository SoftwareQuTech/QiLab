// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <algorithm>
#include <random>
#include "stim.h"
#include "logger/logger_api.hpp"
#include "stim_adapter.hpp"

StimAdapter::StimAdapter(std::seed_seq &seed_seq, const uint8_t &num_qubits)
    : generator(seed_seq), stim_instance(std::move(generator), num_qubits),
    single_target{},
    single_span(single_target.data(), single_target.data() + 1),
    measure_inst(stim::GateType::M, {}, single_span, ""),
    init_inst(stim::GateType::R, {}, single_span, ""),
    h_gate_inst(stim::GateType::H, {}, single_span, ""),
    x_gate_inst(stim::GateType::X, {}, single_span, ""), 
    sqrt_x_gate_inst(stim::GateType::SQRT_X, {}, single_span, ""), 
    x_gate_dag_inst(stim::GateType::SQRT_X_DAG, {}, single_span, ""), 
    y_gate_inst(stim::GateType::Y, {}, single_span, ""), 
    sqrt_y_gate_inst(stim::GateType::SQRT_Y, {}, single_span, ""), 
    y_gate_dag_inst(stim::GateType::SQRT_Y_DAG, {}, single_span, ""), 
    z_gate_inst(stim::GateType::Z, {}, single_span, ""), 
    sqrt_z_gate_inst(stim::GateType::S, {}, single_span, ""), 
    z_gate_dag_inst(stim::GateType::S_DAG, {}, single_span, ""), 
    two_target{}, 
    two_span(two_target.data(), two_target.data() + 2), 
    cnot_inst(stim::GateType::CX, {}, two_span, ""), 
    cx_inst(stim::GateType::CX, {}, two_span, ""), 
    cy_inst(stim::GateType::CY, {}, two_span, ""), 
    cz_inst(stim::GateType::CZ, {}, two_span, ""), 
    swap_inst(stim::GateType::SWAP, {}, two_span, ""),
    sqrt_yy_inst(stim::GateType::SQRT_YY, {}, two_span, ""),
    sqrt_yy_dag_inst(stim::GateType::SQRT_YY_DAG, {}, two_span, ""),
    sqrt_xx_inst(stim::GateType::SQRT_XX, {}, two_span, ""),
    sqrt_xx_dag_inst(stim::GateType::SQRT_XX_DAG, {}, two_span, ""),
    pauli_channel_probs{0.0f, 0.0f, 0.0f},
    pauli_probs_span(pauli_channel_probs.data(), pauli_channel_probs.data() + 3),
    pauli_channel_1_inst(stim::GateType::PAULI_CHANNEL_1, pauli_probs_span, single_span, ""),
    depolarize_2_prob{0.0},
    depolarize_2_prob_span(depolarize_2_prob.data(), depolarize_2_prob.data() + 1),
    depolarize_2_inst(stim::GateType::DEPOLARIZE2, depolarize_2_prob_span, two_span, "")
{
    // Pre-allocate enough space for all qubits
    stim_instance.ensure_large_enough_for_qubits(num_qubits);
    registered_qubits = num_qubits;
}

StimAdapter::~StimAdapter() {}

bool StimAdapter::allocate_qubits(uint8_t num_qubits)
{
    if (num_qubits + registered_qubits > MAX_QUBITS_AVAILABLE)
        return false;
    LOG_INFO("Allocating in STIM [ " + std::to_string(num_qubits) + " ] qubits. Total registered qubits: " + std::to_string(registered_qubits + num_qubits));
    stim_instance.ensure_large_enough_for_qubits(registered_qubits + num_qubits);
    registered_qubits += num_qubits;
    return true;
}

bool StimAdapter::single_qubit_noise(uint8_t qubit_address, float x_noise_probability, float y_noise_probability, float z_noise_probability)
{
    // Update target qubit address
    single_target[0] = stim::GateTarget{qubit_address};

    pauli_channel_probs[0] = static_cast<double>(x_noise_probability);
    pauli_channel_probs[1] = static_cast<double>(y_noise_probability);
    pauli_channel_probs[2] = static_cast<double>(z_noise_probability);
    stim_instance.do_PAULI_CHANNEL_1(pauli_channel_1_inst);
    LOG_INFO("Applied single-qubit noise: qubit=" + std::to_string(qubit_address) + " x_noise=" + std::to_string(x_noise_probability) + " y_noise=" + std::to_string(y_noise_probability) + " z_noise=" + std::to_string(z_noise_probability));
    return true;
}

bool StimAdapter::two_qubit_depolarizing_noise(uint8_t qubit1_address, uint8_t qubit2_address, float depolarizing_probability)
{
    if (qubit1_address == qubit2_address)
    {
        LOG_ERROR("Cannot apply two-qubit depolarizing noise to the same qubit " + std::to_string(qubit1_address));
        return false;
    }

    // Re-stamp the shared two-qubit target array immediately before use, as
    // every other two-qubit instruction in this adapter does.
    two_target[0] = stim::GateTarget{qubit1_address};
    two_target[1] = stim::GateTarget{qubit2_address};
    depolarize_2_prob[0] = static_cast<double>(depolarizing_probability);
    stim_instance.do_DEPOLARIZE2(depolarize_2_inst);
    LOG_INFO("Applied two-qubit depolarizing noise: q1=" + std::to_string(qubit1_address) +
             " q2=" + std::to_string(qubit2_address) + " p=" + std::to_string(depolarizing_probability));
    return true;
}

bool StimAdapter::qubit_initialize(uint8_t qubit_address)
{
    // Use our cached single_target array
    single_target[0] = stim::GateTarget{qubit_address};
    // Use our cached initialization instruction
    stim_instance.do_RZ(init_inst);
    LOG_INFO("Initialized qubit " + std::to_string(qubit_address) + " to |0> state.");
    return true;
}
bool StimAdapter::single_qubit_gate(uint8_t qubit_address, Angle_t angle, Axis_t axis)
{
    // Update target qubit address
    single_target[0] = stim::GateTarget{qubit_address};

    if (angle == Angle_t::_8_PI_16)
    {
        if (axis == Axis_t::X_AXIS)
        {
            LOG_INFO("Applying single-qubit gate: qubit=" + std::to_string(qubit_address) + " angle=" + std::to_string(angle) + " axis=" + std::to_string(axis));
            stim_instance.do_SQRT_X(sqrt_x_gate_inst);
            return true;
        }
        else if (axis == Axis_t::Y_AXIS)
        {
            LOG_INFO("Applying single-qubit gate: qubit=" + std::to_string(qubit_address) + " angle=" + std::to_string(angle) + " axis=" + std::to_string(axis));
            stim_instance.do_SQRT_Y(sqrt_y_gate_inst);
            return true;
        }
        else if (axis == Axis_t::Z_AXIS)
        {
            LOG_INFO("Applying single-qubit gate: qubit=" + std::to_string(qubit_address) + " angle=" + std::to_string(angle) + " axis=" + std::to_string(axis));
            stim_instance.do_SQRT_Z(sqrt_z_gate_inst);
            return true;
        }
        else
        {
            LOG_WARNING("Invalid axis for single-qubit gate: " + std::to_string(axis));
            return false;
        }
    }
    else if (angle == Angle_t::_MINUS_8_PI_16)
    {
        LOG_INFO("Applying single-qubit gate: qubit=" + std::to_string(qubit_address) + " angle=" + std::to_string(angle) + " axis=" + std::to_string(axis));
        if (axis == Axis_t::X_AXIS)
        {
            stim_instance.do_SQRT_X_DAG(x_gate_dag_inst);
            return true;
        }
        else if (axis == Axis_t::Y_AXIS)
        {
            stim_instance.do_SQRT_Y_DAG(y_gate_dag_inst);
            return true;
        }
        else if (axis == Axis_t::Z_AXIS)
        {
            stim_instance.do_SQRT_Z_DAG(z_gate_dag_inst);
            return true;
        }
        else
        {
            LOG_WARNING("Invalid axis for single-qubit gate: " + std::to_string(axis));
            return false;
        }
    }
    else if (angle == Angle_t::_MINUS_PI) // closest to PI
    {
        LOG_INFO("Applying single-qubit gate: qubit=" + std::to_string(qubit_address) + " angle=" + std::to_string(angle) + " axis=" + std::to_string(axis));
        if (axis == Axis_t::X_AXIS)
        {
            stim_instance.do_X(x_gate_inst);
            return true;
        }
        else if (axis == Axis_t::Y_AXIS)
        {
            stim_instance.do_Y(y_gate_inst);
            return true;
        }
        else if (axis == Axis_t::Z_AXIS)
        {
            stim_instance.do_Z(z_gate_inst);
            return true;
        }
        else
        {
            LOG_WARNING("Invalid axis for single-qubit gate: " + std::to_string(axis));
            return false;
        }
    }
    else
    {
        LOG_WARNING("Unsupported angle for single-qubit gate: " + std::to_string(angle));
        return false;
    }

    LOG_ERROR("Failed to apply single-qubit gate: qubit=" + std::to_string(qubit_address) + " angle=" + std::to_string(angle) + " axis=" + std::to_string(axis));
    return false;
}

bool StimAdapter::two_qubit_gate(uint8_t qubit1_address, uint8_t qubit2_address, Angle_t angle, Axis_t axis)
{
    // Validate: cannot apply two-qubit gate to the same qubit
    if (qubit1_address == qubit2_address)
    {
        LOG_ERROR("Cannot apply a two qubit gate to the same qubit 1:  " + std::to_string(qubit1_address) + ", 2: " + std::to_string(qubit2_address));
        return false;
    }

    if (axis == Axis_t::X_AXIS)
    {
        if (angle == Angle_t::_8_PI_16)
        {
            // Update target qubits using cached array
            two_target[0] = stim::GateTarget{qubit1_address};
            two_target[1] = stim::GateTarget{qubit2_address};
            stim_instance.do_SQRT_XX(sqrt_xx_inst); // SQRT_XX is implemented via a custom instruction
            LOG_INFO("Applying two-qubit gate: qubits=" + std::to_string(qubit1_address) + "," + std::to_string(qubit2_address) + " angle=" + std::to_string(angle) + " axis=" + std::to_string(axis));
            return true;
        }
        else if (angle == Angle_t::_MINUS_PI)
        {
            single_target[0] = stim::GateTarget{qubit1_address};
            stim_instance.do_X(x_gate_inst); // X is implemented via a custom instruction
            single_target[0] = stim::GateTarget{qubit2_address};
            stim_instance.do_X(x_gate_inst); // X is implemented via a custom instruction
            LOG_INFO("Applying two-qubit gate: qubits=" + std::to_string(qubit1_address) + "," + std::to_string(qubit2_address) + " angle=" + std::to_string(angle) + " axis=" + std::to_string(axis));
            return true;
        }
        else if (angle == Angle_t::_MINUS_8_PI_16)
        {
            // Update target qubits using cached array
            two_target[0] = stim::GateTarget{qubit1_address};
            two_target[1] = stim::GateTarget{qubit2_address};
            stim_instance.do_SQRT_XX_DAG(sqrt_xx_dag_inst); // SQRT_XX_DAG is implemented via a custom instruction
            LOG_INFO("Applying two-qubit gate: qubits=" + std::to_string(qubit1_address) + "," + std::to_string(qubit2_address) + " angle=" + std::to_string(angle) + " axis=" + std::to_string(axis));
            return true;
        }
        {
            LOG_WARNING("Unsupported angle for two-qubit gate: " + std::to_string(angle) + " on axis: " + std::to_string(axis));
            return false;
        }
    }
    else if (axis == Axis_t::Y_AXIS)
    {
        if (angle == Angle_t::_8_PI_16)
        {
            // Update target qubits using cached array
            two_target[0] = stim::GateTarget{qubit1_address};
            two_target[1] = stim::GateTarget{qubit2_address};
            stim_instance.do_SQRT_YY(sqrt_yy_inst); // SQRT_YY is implemented via a custom instruction
            LOG_INFO("Applying two-qubit gate: qubits=" + std::to_string(qubit1_address) + "," + std::to_string(qubit2_address) + " angle=" + std::to_string(angle) + " axis=" + std::to_string(axis));
            return true;
        }
        else if (angle == Angle_t::_MINUS_PI)
        {
            single_target[0] = stim::GateTarget{qubit1_address};
            stim_instance.do_Y(y_gate_inst); // Y is implemented via a custom instruction
            single_target[0] = stim::GateTarget{qubit2_address};
            stim_instance.do_Y(y_gate_inst); // Y is implemented via a custom instruction
            LOG_INFO("Applying two-qubit gate: qubits=" + std::to_string(qubit1_address) + "," + std::to_string(qubit2_address) + " angle=" + std::to_string(angle) + " axis=" + std::to_string(axis));
            return true;
        }
        else if (angle == Angle_t::_MINUS_8_PI_16)
        {
            // Update target qubits using cached array
            two_target[0] = stim::GateTarget{qubit1_address};
            two_target[1] = stim::GateTarget{qubit2_address};
            stim_instance.do_SQRT_YY_DAG(sqrt_yy_dag_inst); // SQRT_YY_DAG is implemented via a custom instruction
            LOG_INFO("Applying two-qubit gate: qubits=" + std::to_string(qubit1_address) + "," + std::to_string(qubit2_address) + " angle=" + std::to_string(angle) + " axis=" + std::to_string(axis));
            return true;
        }
        {
            LOG_WARNING("Unsupported angle for two-qubit gate: " + std::to_string(angle) + " on axis: " + std::to_string(axis));
            return false;
        }
    }
    else
    {
        LOG_WARNING("Invalid axis for two-qubit gate: " + std::to_string(axis));
        return false;
    }
    return true;
}
uint8_t StimAdapter::qubit_measure(uint8_t qubit_address, bool basis)
{
    // Any basis-rotation (pre-measurement) gates are applied by the caller
    // (SimulatorManager) via single_qubit_gate before this call.

    // Use our cached target array
    single_target[0] = stim::GateTarget{qubit_address};

    // Get current record size for reading result
    const size_t base = stim_instance.measurement_record.storage.size();

    // Use our cached instruction (span and targets already set up)
    stim_instance.do_MZ(measure_inst);

    bool measurement = (stim_instance.measurement_record.storage[base] != 0);

    // Only clear if we've accumulated too many measurements
    if (stim_instance.measurement_record.storage.size() > MEASUREMENT_CLEAR_THRESHOLD)
    {
        stim_instance.measurement_record.clear();
    }

    LOG_INFO("Measured qubit " + std::to_string(qubit_address) + " in " + (basis ? "X" : "Z") + "-basis with result: " + std::to_string(measurement));
    return measurement;
}
int8_t StimAdapter::attempt_entanglement(uint16_t fidelity, uint8_t local_qubit, uint8_t neighbour_address, bool is_initiator, uint8_t qid)
{
    // Validate: cannot entangle a qubit with itself
    if (local_qubit == neighbour_address)
    {
        LOG_ERROR("Cannot entangle qubit " + std::to_string(local_qubit) + " with itself!");
        return -1;
    }

    // Ensure qubits are allocated in the simulator
    size_t needed_qubits = std::max(local_qubit, neighbour_address) + 1;
    stim_instance.ensure_large_enough_for_qubits(needed_qubits);

    // ONLY the initiator node applies the entangling gates
    if (is_initiator)
    {
        // Force initialize both qubits to |0⟩
        single_target[0] = stim::GateTarget{local_qubit};
        stim_instance.do_RZ(init_inst);
        single_target[0] = stim::GateTarget{neighbour_address};
        stim_instance.do_RZ(init_inst);

        // Use cached single_target for H gate
        single_target[0] = stim::GateTarget{local_qubit};
        stim_instance.do_H_XZ(h_gate_inst);

        // needed in order to produce psi+ state
        // TODO: make this random as to produce different bell states
        single_target[0] = stim::GateTarget{neighbour_address};
        stim_instance.do_X(x_gate_inst);

        // Use cached two_target for CX gate
        two_target[0] = stim::GateTarget{local_qubit};
        two_target[1] = stim::GateTarget{neighbour_address};
        stim_instance.do_ZCX(cnot_inst);
        LOG_INFO("attempt_entanglement (initiator): Applied H+CNOT to Q" + std::to_string(local_qubit) + ",Q" + std::to_string(neighbour_address));
    }
    else
    {
        LOG_INFO("attempt_entanglement (responder): Skipping gate application, just checking observables");
    }

    // Create XX observable using from_func (no string parsing!)
    size_t max_qubit = std::max(local_qubit, neighbour_address) + 1;
    auto xx_obs = stim::PauliString<64>::from_func(
        false, // positive sign
        max_qubit,
        [local_qubit, neighbour_address](size_t q)
        {
            if (q == local_qubit || q == neighbour_address)
                return 'X';
            return 'I';
        });

    // Create ZZ observable using from_func (no string parsing!)
    auto zz_obs = stim::PauliString<64>::from_func(
        false, // positive sign
        max_qubit,
        [local_qubit, neighbour_address](size_t q)
        {
            if (q == local_qubit || q == neighbour_address)
                return 'Z';
            return 'I';
        });

    // Determine Bell state: XX=+1,ZZ=+1 → |Φ+⟩, XX=-1,ZZ=+1 → |Φ-⟩, XX=+1,ZZ=-1 → |Ψ+⟩, XX=-1,ZZ=-1 → |Ψ-⟩
    int8_t xx_result = stim_instance.peek_observable_expectation(xx_obs);
    int8_t zz_result = stim_instance.peek_observable_expectation(zz_obs);
    LOG_INFO("attempt_entanglement Q" + std::to_string(local_qubit) + ",Q" + std::to_string(neighbour_address) +
             ": XX=" + std::to_string(xx_result) + " ZZ=" + std::to_string(zz_result));
    if (xx_result == 1 && zz_result == 1)
    {
        return 0; // |Φ+⟩
    }
    else if (xx_result == -1 && zz_result == 1)
    {
        return 1; // |Φ-⟩
    }
    else if (xx_result == 1 && zz_result == -1)
    {
        return 2; // |Ψ+⟩
    }
    else if (xx_result == -1 && zz_result == -1)
    {
        return 3; // |Ψ-⟩
    }

    LOG_ERROR("attempt_entanglement: Undefined Bell state - XX=" + std::to_string(xx_result) + " ZZ=" + std::to_string(zz_result));
    return -1;
}
bool StimAdapter::qubit_move(uint8_t qubit1_address, uint8_t qubit2_address)
{
    // assert(("This function is not implemented", 0));
    LOG_ERROR("qubit_move is not implemented yet!");
    return false;
}
bool StimAdapter::qubit_swap(uint8_t qubit1_address, uint8_t qubit2_address)
{
    // Validate: cannot entangle a qubit with itself
    if (qubit1_address == qubit2_address)
    {
        LOG_ERROR("Cannot swap qubit : " + std::to_string(qubit1_address) + " with itself!");
        return false;
    }
    // Use cached two_target for SWAP gate
    two_target[0] = stim::GateTarget{qubit1_address};
    two_target[1] = stim::GateTarget{qubit2_address};
    stim_instance.do_SWAP(swap_inst);
    LOG_INFO("Swapped qubits " + std::to_string(qubit1_address) + " and " + std::to_string(qubit2_address));
    return true;
}
bool StimAdapter::entanglement_swap(uint8_t qubit1_address, uint8_t qubit2_address)
{
    // Use cached two_target for CX gate
    // two_target[0] = stim::GateTarget{qubit1_address};
    // two_target[1] = stim::GateTarget{qubit2_address};
    // stim_instance.do_ZCX(cnot_inst);

    // // Use cached single_target for H gate
    // single_target[0] = stim::GateTarget{qubit1_address};
    // stim_instance.do_H_XZ(h_gate_inst);
    LOG_ERROR("entanglement_swap is not implemented yet!");
    return false;
}
