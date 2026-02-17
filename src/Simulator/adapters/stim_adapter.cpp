#include <random>
#include "stim.h"
#include "logger/logger_api.hpp"
#include "stim_adapter.hpp"

StimAdapter::StimAdapter(std::seed_seq &seed_seq, const uint8_t &num_qubits) 
    : generator(seed_seq)
    , stim_instance(std::move(generator), num_qubits)
    , single_target{}
    , single_span(single_target.data(), single_target.data() + 1)
    , measure_inst(stim::GateType::M, {}, single_span, "")
    , init_inst(stim::GateType::R, {}, single_span, "")
    , h_gate_inst(stim::GateType::H, {}, single_span, "")
    , x_gate_inst(stim::GateType::X, {}, single_span, "")
    , sqrt_x_gate_inst(stim::GateType::SQRT_X, {}, single_span, "")
    , x_gate_dag_inst(stim::GateType::SQRT_X_DAG, {}, single_span, "")
    , y_gate_inst(stim::GateType::Y, {}, single_span, "")
    , sqrt_y_gate_inst(stim::GateType::SQRT_Y, {}, single_span, "")
    , y_gate_dag_inst(stim::GateType::SQRT_Y_DAG, {}, single_span, "")
    , z_gate_inst(stim::GateType::Z, {}, single_span, "")
    , sqrt_z_gate_inst(stim::GateType::S, {}, single_span, "")
    , z_gate_dag_inst(stim::GateType::S_DAG, {}, single_span, "")
    , two_target{}
    , two_span(two_target.data(), two_target.data() + 2)
    , cnot_inst(stim::GateType::CX, {}, two_span, "")
    , cz_inst(stim::GateType::CZ, {}, two_span, "")
    , swap_inst(stim::GateType::SWAP, {}, two_span, "")
{
    // Pre-allocate enough space for all qubits
    stim_instance.ensure_large_enough_for_qubits(num_qubits);
    registered_qubits = num_qubits;
}



StimAdapter::~StimAdapter() {}

bool StimAdapter::allocate_qubits(uint8_t num_qubits)
{
    if(num_qubits + registered_qubits > MAX_QUBITS_AVAILABLE)
        return false;

    stim_instance.ensure_large_enough_for_qubits(registered_qubits + num_qubits);
    registered_qubits += num_qubits;
    return true;
}

bool StimAdapter::qubit_initialize(uint8_t qubit_address)
{
    // Use our cached single_target array
    single_target[0] = stim::GateTarget{qubit_address};

    // Use our cached initialization instruction
    stim_instance.do_RZ(init_inst);

    return true;
}
bool StimAdapter::single_qubit_gate(uint8_t qubit_address, Angle_t angle, Axis_t axis)
{
    // Update target qubit address
    single_target[0] = stim::GateTarget{qubit_address};

    if (angle == Angle_t::_8_PI_16)
    {
        if (axis == Axis_t::X_AXIS) {
            stim_instance.do_SQRT_X(sqrt_x_gate_inst);
            return true;
        }
        else if (axis == Axis_t::Y_AXIS) {
            stim_instance.do_SQRT_Y(sqrt_y_gate_inst);
            return true;
        }
        else if (axis == Axis_t::Z_AXIS) {
            stim_instance.do_SQRT_Z(sqrt_z_gate_inst);
            return true;
        }
        else
            return false;
    }
    else if (angle == Angle_t::_MINUS_8_PI_16)
    {
        if (axis == Axis_t::X_AXIS) {
            stim_instance.do_SQRT_X_DAG(x_gate_dag_inst);
            return true;
        }
        else if (axis == Axis_t::Y_AXIS) {
            stim_instance.do_SQRT_Y_DAG(y_gate_dag_inst);
            return true;
        }
        else if (axis == Axis_t::Z_AXIS) {
            stim_instance.do_SQRT_Z_DAG(z_gate_dag_inst);
            return true;
        }
        else
            return false;
    }
    else if (angle == Angle_t::_15_PI_16) // closest to PI
    {
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
            return false;
    }
    else
        return false;

    return false;
}

bool StimAdapter::two_qubit_gate(uint8_t qubit1_address, uint8_t qubit2_address, Angle_t angle, Axis_t axis)
{
    // Validate: cannot apply two-qubit gate to the same qubit
    if (qubit1_address == qubit2_address) {
        LOG_ERROR("Cannot apply a two qubit gate to the same qubit 1:  " + std::to_string(qubit1_address) + ", 2: " + std::to_string(qubit2_address));
        return false;
    }
    // Update target qubits using cached array
    two_target[0] = stim::GateTarget{qubit1_address};
    two_target[1] = stim::GateTarget{qubit2_address};

    if (axis == Axis_t::X_AXIS) {
        stim_instance.do_ZCX(cnot_inst);  // CNOT is implemented via ZCX
    }
    else if (axis == Axis_t::Z_AXIS) {
        stim_instance.do_ZCZ(cz_inst);    // CZ is implemented via ZCZ
    }
    else {
        return false;
    }

    return true;
}
uint8_t StimAdapter::qubit_measure(uint8_t qubit_address, bool basis)
{
    // Apply pre-measurement gates configured for regular measurements (target=0)
    apply_pre_measure_gates(qubit_address, pre_measure_config);
    pre_measure_config.enabled = false; // Reset after applying

    // Use our cached target array
    single_target[0] = stim::GateTarget{qubit_address};
    
    // Get current record size for reading result
    const size_t base = stim_instance.measurement_record.storage.size();
    
    // Use our cached instruction (span and targets already set up)
    stim_instance.do_MZ(measure_inst);
    
    bool measurement = (stim_instance.measurement_record.storage[base] != 0);
    
    // Only clear if we've accumulated too many measurements
    if (stim_instance.measurement_record.storage.size() > MEASUREMENT_CLEAR_THRESHOLD) {
        stim_instance.measurement_record.clear();
    }
    
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
    if (is_initiator) {
        // Force initialize both qubits to |0⟩
        single_target[0] = stim::GateTarget{local_qubit};
        stim_instance.do_RZ(init_inst);
        single_target[0] = stim::GateTarget{neighbour_address};
        stim_instance.do_RZ(init_inst);

        // Use cached single_target for H gate
        single_target[0] = stim::GateTarget{local_qubit};
        stim_instance.do_H_XZ(h_gate_inst);

        //needed in order to produce psi+ state
        //TODO: make this random as to produce different bell states
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
        false,  // positive sign
        max_qubit,
        [local_qubit, neighbour_address](size_t q) {
            if (q == local_qubit || q == neighbour_address) return 'X';
            return 'I';
        }
    );

    // Create ZZ observable using from_func (no string parsing!)
    auto zz_obs = stim::PauliString<64>::from_func(
        false,  // positive sign
        max_qubit,
        [local_qubit, neighbour_address](size_t q) {
            if (q == local_qubit || q == neighbour_address) return 'Z';
            return 'I';
        }
    );

    // Determine Bell state: XX=+1,ZZ=+1 → |Φ+⟩, XX=-1,ZZ=+1 → |Φ-⟩, XX=+1,ZZ=-1 → |Ψ+⟩, XX=-1,ZZ=-1 → |Ψ-⟩
    int8_t xx_result = stim_instance.peek_observable_expectation(xx_obs);
    int8_t zz_result = stim_instance.peek_observable_expectation(zz_obs);
    LOG_INFO("attempt_entanglement Q" + std::to_string(local_qubit) + ",Q" + std::to_string(neighbour_address) + 
             ": XX=" + std::to_string(xx_result) + " ZZ=" + std::to_string(zz_result));
    if(xx_result == 1 && zz_result == 1) {
        return 0; // |Φ+⟩
    } else if (xx_result == -1 && zz_result == 1) {
        return 1; // |Φ-⟩
    } else if (xx_result == 1 && zz_result == -1) {
        return 2; // |Ψ+⟩
    } else if (xx_result == -1 && zz_result == -1) {
        return 3; // |Ψ-⟩
    }

    LOG_ERROR("attempt_entanglement: Undefined Bell state - XX=" + std::to_string(xx_result) + " ZZ=" + std::to_string(zz_result));
    return -1;
}
int8_t StimAdapter::entanglement_measure(uint16_t fidelity, uint8_t local_qubit, uint8_t neighbour_address, bool basis, bool is_initiator, uint8_t qid, bool try_now, uint8_t repetitions)
{
    // Validate: cannot entangle a qubit with itself
    if (local_qubit == neighbour_address) {
        LOG_ERROR("Cannot entangle qubit " + std::to_string(local_qubit) + " with itself!");
        return -1;
    }
    
    // if (!try_now) {
    //     return 0;  // Deferred measurement not requested
    // }

    // Ensure qubits are allocated in the simulator
    size_t needed_qubits = std::max(local_qubit, neighbour_address) + 1;
    stim_instance.ensure_large_enough_for_qubits(needed_qubits);
    
    // ONLY the initiator node applies the entangling gates
    if (is_initiator) {
        // Force initialize both qubits to |0⟩ (they might be in some state from previous operations)
        single_target[0] = stim::GateTarget{local_qubit};
        stim_instance.do_RZ(init_inst);
        single_target[0] = stim::GateTarget{neighbour_address};
        stim_instance.do_RZ(init_inst);

        // First, establish entanglement with neighbor
        // Apply H to local qubit
        single_target[0] = stim::GateTarget{local_qubit};
        stim_instance.do_H_XZ(h_gate_inst);

        //needed in order to produce psi+ state
        //TODO: make this random as to produce different bell states
        single_target[0] = stim::GateTarget{neighbour_address};
        stim_instance.do_X(x_gate_inst);

        // Apply CNOT between local qubit and neighbor
        two_target[0] = stim::GateTarget{local_qubit};
        two_target[1] = stim::GateTarget{neighbour_address};
        stim_instance.do_ZCX(cnot_inst);
        LOG_INFO("entanglement_measure (initiator): Applied H+CNOT to Q" + std::to_string(local_qubit) + ",Q" + std::to_string(neighbour_address));
    } 
    else 
    {
        LOG_INFO("entanglement_measure (responder): Skipping H+CNOT, just checking observables and measuring");
    }

    // Create XX observable using from_func (no string parsing!)
    size_t max_qubit = std::max(local_qubit, neighbour_address) + 1;
    auto xx_obs = stim::PauliString<64>::from_func(
        false,  // positive sign
        max_qubit,
        [local_qubit, neighbour_address](size_t q) {
            if (q == local_qubit || q == neighbour_address) return 'X';
            return 'I';
        }
    );

    // Create ZZ observable using from_func (no string parsing!)
    auto zz_obs = stim::PauliString<64>::from_func(
        false,  // positive sign
        max_qubit,
        [local_qubit, neighbour_address](size_t q) {
            if (q == local_qubit || q == neighbour_address) return 'Z';
            return 'I';
        }
    );

    // Determine Bell state BEFORE applying pre-measurement gates!
    // Pre-measurement gates change the basis, so check the Bell state first
    // XX=+1,ZZ=+1 → |Φ+⟩, XX=-1,ZZ=+1 → |Φ-⟩, XX=+1,ZZ=-1 → |Ψ+⟩, XX=-1,ZZ=-1 → |Ψ-⟩
    int8_t xx_result = stim_instance.peek_observable_expectation(xx_obs);
    int8_t zz_result = stim_instance.peek_observable_expectation(zz_obs);
    LOG_INFO("entanglement_measure Q" + std::to_string(local_qubit) + ",Q" + std::to_string(neighbour_address) + 
             ": Bell state XX=" + std::to_string(xx_result) + " ZZ=" + std::to_string(zz_result) + " (before pre-measure gates)");

    // NOW apply pre-measurement gates (these change the measurement basis, not the Bell state we're identifying)
    // These gates are applied AFTER Bell state identification, BEFORE measurement
    apply_pre_measure_gates(local_qubit, pre_entangle_measure_config);
    pre_entangle_measure_config.enabled = false; // Reset after applying

    // NOW measure the qubit 
    single_target[0] = stim::GateTarget{local_qubit};
    const size_t base = stim_instance.measurement_record.storage.size();
    stim_instance.do_MZ(measure_inst);

    int8_t measurement = (stim_instance.measurement_record.storage[base] != 0);
    
    // Clean up measurement record if threshold exceeded
    if (stim_instance.measurement_record.storage.size() > MEASUREMENT_CLEAR_THRESHOLD) {
        stim_instance.measurement_record.clear();
    }
    
    // Encode result: bits [1:0] = Bell state (0-3), bit [2] = measurement result
    int8_t bell_state;
    if(xx_result == 1 && zz_result == 1) {
        bell_state = 0; // |Φ+⟩
    } else if (xx_result == -1 && zz_result == 1) {
        bell_state = 1; // |Φ-⟩
    } else if (xx_result == 1 && zz_result == -1) {
        bell_state = 2; // |Ψ+⟩
    } else if (xx_result == -1 && zz_result == -1) {
        bell_state = 3; // |Ψ-⟩
    } else {
        LOG_ERROR("entanglement_measure: Undefined Bell state - XX=" + std::to_string(xx_result) + " ZZ=" + std::to_string(zz_result));
        return -1; // Error/undefined state
    }
    
    // Combine Bell state (bits 1:0) with measurement result (bit 2)
    int8_t result = (bell_state & 0x03) | ((measurement & 0x01) << 2);
    LOG_INFO("entanglement_measure: Result=" + std::to_string(result) + " (Bell=" + std::to_string(bell_state) + ", M=" + std::to_string(measurement) + ")");
    return result;
}
void StimAdapter::pre_measure_gates_selection(bool target, Angle_t angle_1, Angle_t angle_2, Angle_t angle_3)
{
    // target=0: store for regular measurements (qubit_measure)
    // target=1: store for entanglement measurements (entanglement_measure)
    PreMeasureConfig& config = target ? pre_entangle_measure_config : pre_measure_config;
    
    config.enabled = true;
    config.angle1 = angle_1;
    config.angle2 = angle_2;
    config.angle3 = angle_3;
}

void StimAdapter::apply_pre_measure_gates(uint8_t qubit_address, const PreMeasureConfig& config)
{
    if (!config.enabled) {
        return;
    }
    
    // Apply X-Y-X rotation sequence
    single_qubit_gate(qubit_address, config.angle1, Axis_t::X_AXIS);
    single_qubit_gate(qubit_address, config.angle2, Axis_t::Y_AXIS);
    single_qubit_gate(qubit_address, config.angle3, Axis_t::X_AXIS);
    LOG_INFO("Applying pre-measurement gates to Q" + std::to_string(qubit_address) + ": X(" + std::to_string(config.angle1) + ") Y(" + std::to_string(config.angle2) + ") X(" + std::to_string(config.angle3) + ")");
}
bool StimAdapter::qubit_move(uint8_t qubit1_address, uint8_t qubit2_address)
{
    //assert(("This function is not implemented", 0));
    LOG_ERROR("qubit_move is not implemented yet!");
    return false;
}
bool StimAdapter::qubit_swap(uint8_t qubit1_address, uint8_t qubit2_address)
{
    // Validate: cannot entangle a qubit with itself
    if (qubit1_address == qubit2_address) {
        LOG_ERROR("Cannot swap qubit : " + std::to_string(qubit1_address) + " with itself!");
        return false;
    }
    // Use cached two_target for SWAP gate
    two_target[0] = stim::GateTarget{qubit1_address};
    two_target[1] = stim::GateTarget{qubit2_address};
    stim_instance.do_SWAP(swap_inst);
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

