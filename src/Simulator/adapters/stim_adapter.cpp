#include <random>
#include "stim.h"

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
    // Update target qubits using cached array
    two_target[0] = stim::GateTarget{qubit1_address};
    two_target[1] = stim::GateTarget{qubit2_address};

    if (axis == Axis_t::X_AXIS) {
        stim_instance.do_ZCX(cnot_inst);  // CNOT is implemented via ZCX
    }
    else if (axis == Axis_t::Z_AXIS) {
        stim_instance.do_ZCZ(cz_inst);    // CZ is implemented via ZCZ
    }
    // Add support for SWAP if needed
    // else if (axis == Axis_t::SWAP) {
    //     stim_instance.do_SWAP(swap_inst);
    // }
    else {
        return false;
    }

    return true;
}
uint8_t StimAdapter::qubit_measure(uint8_t qubit_address, bool basis)
{
    
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
uint8_t StimAdapter::attempt_entanglement(uint16_t fidelity, uint8_t local_qubit, uint8_t neighbour_address, uint8_t qid)
{
    stim_instance.ensure_large_enough_for_qubits(std::max(qid, neighbour_address) + 1);

    // Use cached single_target for H gate
    single_target[0] = stim::GateTarget{local_qubit};
    stim_instance.do_H_XZ(h_gate_inst);

    // Use cached two_target for CX gate
    two_target[0] = stim::GateTarget{local_qubit};
    two_target[1] = stim::GateTarget{neighbour_address};
    stim_instance.do_ZCX(cnot_inst);
    return 0;
}
uint8_t StimAdapter::entanglement_measure(uint16_t fidelity, uint8_t neighbour_address, bool basis, uint8_t qid, bool try_now, uint8_t repetitions)
{
    return 0;//assert(("This function is not implemented", 0));
}
void StimAdapter::pre_measure_gates_selection(bool target, Angle_t angle_1, Angle_t angle_2, Angle_t angle_3)
{
    //assert(("This function is not implemented", 0));
}
void StimAdapter::qubit_move(uint8_t qubit1_address, uint8_t qubit2_address)
{
    //assert(("This function is not implemented", 0));
}
void StimAdapter::qubit_swap(uint8_t qubit1_address, uint8_t qubit2_address)
{
    stim_instance.ensure_large_enough_for_qubits(std::max(qubit1_address, qubit2_address) + 1);
    // Use cached two_target for SWAP gate
    two_target[0] = stim::GateTarget{qubit1_address};
    two_target[1] = stim::GateTarget{qubit2_address};
    stim_instance.do_SWAP(swap_inst);
}
uint8_t StimAdapter::entanglement_swap(uint8_t qubit1_address, uint8_t qubit2_address)
{
    stim_instance.ensure_large_enough_for_qubits(std::max(qubit1_address, qubit2_address) + 1);

    // Use cached two_target for CX gate
    two_target[0] = stim::GateTarget{qubit1_address};
    two_target[1] = stim::GateTarget{qubit2_address};
    stim_instance.do_ZCX(cnot_inst);

    // Use cached single_target for H gate
    single_target[0] = stim::GateTarget{qubit1_address};
    stim_instance.do_H_XZ(h_gate_inst);
    
    return 0;
}
