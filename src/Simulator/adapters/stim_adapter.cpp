#include <random>
#include "stim.h"

#include "stim_adapter.hpp"

StimAdapter::StimAdapter(std::seed_seq &seed_seq, const uint8_t &num_qubits) : generator(seed_seq), stim_instance(std::move(generator), num_qubits)
{
    //stim_instance.set_num_qubits(num_qubits);
}

StimAdapter::StimAdapter(std::seed_seq &seed_seq, const uint8_t &num_qubits, IProfiler *logger) : generator(seed_seq), stim_instance(std::move(generator), num_qubits)
{
    //stim_instance.set_num_qubits(num_qubits);
}

StimAdapter::~StimAdapter() {}

bool StimAdapter::qubit_initialize(uint8_t qubit_address)
{
    stim_instance.ensure_large_enough_for_qubits(qubit_address + 1);

    std::array<stim::GateTarget, 1> a{stim::GateTarget{qubit_address}};
    stim::SpanRef<stim::GateTarget> span(a.data(), a.data() + a.size());
    stim::CircuitInstruction inst{stim::GateType::R, {}, span, ""};
    stim_instance.do_RZ(inst);

    return true;
}
bool StimAdapter::single_qubit_gate(uint8_t qubit_address, Angle_t angle, Axis_t axis)
{
    if (angle == Angle_t::_8_PI_16)
    {
        if (axis == Axis_t::X_AXIS)
        {
            stim_instance.ensure_large_enough_for_qubits(qubit_address + 1);
            std::array<stim::GateTarget, 1> a{stim::GateTarget{qubit_address}};
            stim::SpanRef<stim::GateTarget> span(a.data(), a.data() + a.size());
            stim::CircuitInstruction inst{stim::GateType::SQRT_X, {}, span, ""};
            stim_instance.do_SQRT_X(inst);
            return true;
        }
        else if (axis == Axis_t::Y_AXIS)
        {
            stim_instance.ensure_large_enough_for_qubits(qubit_address + 1);
            std::array<stim::GateTarget, 1> a{stim::GateTarget{qubit_address}};
            stim::SpanRef<stim::GateTarget> span(a.data(), a.data() + a.size());
            stim::CircuitInstruction inst{stim::GateType::SQRT_Y, {}, span, ""};
            stim_instance.do_SQRT_Y(inst);
            return true;
        }
        else if (axis == Axis_t::Z_AXIS)
        {
            stim_instance.ensure_large_enough_for_qubits(qubit_address + 1);
            std::array<stim::GateTarget, 1> a{stim::GateTarget{qubit_address}};
            stim::SpanRef<stim::GateTarget> span(a.data(), a.data() + a.size());
            stim::CircuitInstruction inst{stim::GateType::S, {}, span, ""}; // as per py bindings
            stim_instance.do_SQRT_Z(inst);
            return true;
        }
        else
            return false;
    }
    else if (angle == Angle_t::_MINUS_8_PI_16)
    {
        if (axis == Axis_t::X_AXIS)
        {
            stim_instance.ensure_large_enough_for_qubits(qubit_address + 1);
            std::array<stim::GateTarget, 1> a{stim::GateTarget{qubit_address}};
            stim::SpanRef<stim::GateTarget> span(a.data(), a.data() + a.size());
            stim::CircuitInstruction inst{stim::GateType::SQRT_X_DAG, {}, span, ""};
            stim_instance.do_SQRT_X_DAG(inst);
            return true;
        }
        else if (axis == Axis_t::Y_AXIS)
        {
            stim_instance.ensure_large_enough_for_qubits(qubit_address + 1);
            std::array<stim::GateTarget, 1> a{stim::GateTarget{qubit_address}};
            stim::SpanRef<stim::GateTarget> span(a.data(), a.data() + a.size());
            stim::CircuitInstruction inst{stim::GateType::SQRT_Y_DAG, {}, span, ""};
            stim_instance.do_SQRT_Y_DAG(inst);
            return true;
        }
        else if (axis == Axis_t::Z_AXIS)
        {
            stim_instance.ensure_large_enough_for_qubits(qubit_address + 1);
            std::array<stim::GateTarget, 1> a{stim::GateTarget{qubit_address}};
            stim::SpanRef<stim::GateTarget> span(a.data(), a.data() + a.size());
            stim::CircuitInstruction inst{stim::GateType::S_DAG, {}, span, ""};
            stim_instance.do_SQRT_Z_DAG(inst);
        }
        else
            return false;
    }
    else if (angle == Angle_t::_15_PI_16) // closest to PI
    {
        if (axis == Axis_t::X_AXIS)
        {
            stim_instance.ensure_large_enough_for_qubits(qubit_address + 1);
            std::array<stim::GateTarget, 1> a{stim::GateTarget{qubit_address}};
            stim::SpanRef<stim::GateTarget> span(a.data(), a.data() + a.size());
            stim::CircuitInstruction inst{stim::GateType::X, {}, span, ""};
            stim_instance.do_X(inst);
            return true;
        }
        else if (axis == Axis_t::Y_AXIS)
        {
            stim_instance.ensure_large_enough_for_qubits(qubit_address + 1);
            std::array<stim::GateTarget, 1> a{stim::GateTarget{qubit_address}};
            stim::SpanRef<stim::GateTarget> span(a.data(), a.data() + a.size());
            stim::CircuitInstruction inst{stim::GateType::Y, {}, span, ""};
            stim_instance.do_Y(inst);
            return true;
        }
        else if (axis == Axis_t::Z_AXIS)
        {
            stim_instance.ensure_large_enough_for_qubits(qubit_address + 1);
            std::array<stim::GateTarget, 1> a{stim::GateTarget{qubit_address}};
            stim::SpanRef<stim::GateTarget> span(a.data(), a.data() + a.size());
            stim::CircuitInstruction inst{stim::GateType::Z, {}, span, ""};
            stim_instance.do_Z(inst);
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
    stim_instance.ensure_large_enough_for_qubits(std::max(qubit1_address, qubit2_address) + 1);
    std::array<stim::GateTarget, 2> a{stim::GateTarget{qubit1_address}, stim::GateTarget{qubit2_address}};
    stim::SpanRef<stim::GateTarget> span(a.data(), a.data() + a.size());
    stim::CircuitInstruction inst{stim::GateType::CX, {}, span, ""};
    stim_instance.do_ZCX(inst);

    return true;
}
uint8_t StimAdapter::qubit_measure(uint8_t qubit_address, bool basis)
{
    stim_instance.ensure_large_enough_for_qubits(qubit_address + 1);

    const size_t base = stim_instance.measurement_record.storage.size();

    std::array<stim::GateTarget, 1> a{stim::GateTarget{qubit_address}};
    stim::SpanRef<stim::GateTarget> span(a.data(), a.data() + a.size());
    stim::CircuitInstruction inst{stim::GateType::M, {}, span, ""};
    stim_instance.do_MZ(inst);

    bool measurement = (stim_instance.measurement_record.storage[base] != 0);

    // clear the record, no need to increase indefinitely
    stim_instance.measurement_record.clear();

    return measurement;
}
uint8_t StimAdapter::attempt_entanglement(uint16_t fidelity, uint8_t neighbour_address, uint8_t qid)
{
    stim_instance.ensure_large_enough_for_qubits(std::max(qid, neighbour_address) + 1);

    std::array<stim::GateTarget, 1> a{stim::GateTarget{qid}};
    stim::SpanRef<stim::GateTarget> span(a.data(), a.data() + a.size());
    stim::CircuitInstruction inst{stim::GateType::H, {}, span, ""};
    stim_instance.do_H_XZ(inst);

    std::array<stim::GateTarget, 2> b{stim::GateTarget{qid}, stim::GateTarget{neighbour_address}};
    stim::SpanRef<stim::GateTarget> span1(b.data(), b.data() + b.size());
    stim::CircuitInstruction inst1{stim::GateType::CX, {}, span1, ""};
    stim_instance.do_ZCX(inst1);
    return 0;
}
uint8_t StimAdapter::entanglement_measure(uint16_t fidelity, uint8_t neighbour_address, bool basis, uint8_t qid, bool try_now, uint8_t repetitions)
{
    return 0;
}
void StimAdapter::pre_measure_gates_selection(bool target, Angle_t angle_1, Angle_t angle_2, Angle_t angle_3)
{
    assert(("This function is not implemented", 0));
}
void StimAdapter::qubit_move(uint8_t qubit1_address, uint8_t qubit2_address)
{
    assert(("This function is not implemented", 0));
}
void StimAdapter::qubit_swap(uint8_t qubit1_address, uint8_t qubit2_address)
{
    stim_instance.ensure_large_enough_for_qubits(std::max(qubit1_address, qubit2_address) + 1);
    std::array<stim::GateTarget, 2> a{stim::GateTarget{qubit1_address}, stim::GateTarget{qubit2_address}};
    stim::SpanRef<stim::GateTarget> span(a.data(), a.data() + a.size());
    stim::CircuitInstruction inst{stim::GateType::SWAP, {}, span, ""};
    stim_instance.do_SWAP(inst);
}
uint8_t StimAdapter::entanglement_swap(uint8_t qubit1_address, uint8_t qubit2_address)
{
    stim_instance.ensure_large_enough_for_qubits(std::max(qubit1_address, qubit2_address) + 1);

    std::array<stim::GateTarget, 2> b{stim::GateTarget{qubit1_address}, stim::GateTarget{qubit2_address}};
    stim::SpanRef<stim::GateTarget> span1(b.data(), b.data() + b.size());
    stim::CircuitInstruction inst1{stim::GateType::CX, {}, span1, ""};
    stim_instance.do_ZCX(inst1);

    std::array<stim::GateTarget, 1> a{stim::GateTarget{qubit1_address}};
    stim::SpanRef<stim::GateTarget> span(a.data(), a.data() + a.size());
    stim::CircuitInstruction inst{stim::GateType::H, {}, span, ""};
    stim_instance.do_H_XZ(inst);
    
    return 0;
}
