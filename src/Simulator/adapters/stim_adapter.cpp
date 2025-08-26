#include <random>
#include "stim.h"

#include "stim_adapter.hpp"

StimAdapter::StimAdapter(const uint64_t &seed, const uint8_t &num_qubits) : rng(seed), stim_instance(std::move(rng))
{
    stim_instance.set_num_qubits(num_qubits);
}

StimAdapter::StimAdapter(const uint64_t &seed, const uint8_t &num_qubits, ILogger* logger) : rng(seed), stim_instance(std::move(rng))
{
    stim_instance.set_num_qubits(num_qubits);
}

StimAdapter::~StimAdapter() {}


bool StimAdapter::qubit_initialize(uint8_t qubit_address) 
{
    //TODO: check if this exepts!!
    stim_instance.ensure_large_enough_for_qubits(qubit_address + 1);
    stim::GateTarget g{qubit_address};
    stim::CircuitInstruction inst{stim::GateType::M, {}, &g, ""};
    stim_instance.do_MZ(inst); // performs Z-measure & records bit
    if ((bool)stim_instance.measurement_record.storage.back())
    {
        stim::CircuitInstruction reset_{stim::GateType::X, {}, &g, ""};
        stim_instance.do_X(reset_);
    }

    return true;
}
bool StimAdapter::single_qubit_gate(uint8_t qubit_address, Angle_t angle, Axis_t axis)
{
    if(angle == Angle_t::_8_PI_16)
    {
        if(axis == Axis_t::X_AXIS)
        {
            //TODO: check if this exepts!
            stim_instance.ensure_large_enough_for_qubits(qubit_address + 1);
            stim::GateTarget g{qubit_address};
            stim::CircuitInstruction inst{stim::GateType::SQRT_X, {}, &g, ""};
            stim_instance.do_SQRT_X(inst);
            return true;
        }
        else if(axis == Axis_t::Y_AXIS)
        {
            //TODO: check if this exepts!
            stim_instance.ensure_large_enough_for_qubits(qubit_address + 1);
            stim::GateTarget g{qubit_address};
            stim::CircuitInstruction inst{stim::GateType::SQRT_Y, {}, &g, ""};
            stim_instance.do_SQRT_Y(inst);
            return true;
        }
        else if(axis == Axis_t::Z_AXIS)
        {
            //TODO: check if this exepts!
            stim_instance.ensure_large_enough_for_qubits(qubit_address + 1);
            stim::GateTarget g{qubit_address};
            stim::CircuitInstruction inst{stim::GateType::S, {}, &g, ""};
            stim_instance.do_SPP(inst);
            return true;
        }
        else
            return false;
    }
    else if(angle == Angle_t::_MINUS_8_PI_16)
    {
        if(axis == Axis_t::X_AXIS)
        {
            //TODO: check if this exepts!
            stim_instance.ensure_large_enough_for_qubits(qubit_address + 1);
            stim::GateTarget g{qubit_address};
            stim::CircuitInstruction inst{stim::GateType::SQRT_X_DAG, {}, &g, ""};
            stim_instance.do_SQRT_X_DAG(inst);
            return true;
        }
        else if(axis == Axis_t::Y_AXIS)
        {
            //TODO: check if this exepts!
            stim_instance.ensure_large_enough_for_qubits(qubit_address + 1);
            stim::GateTarget g{qubit_address};
            stim::CircuitInstruction inst{stim::GateType::SQRT_Y_DAG, {}, &g, ""};
            stim_instance.do_SQRT_Y_DAG(inst);
            return true;
        }
        else if(axis == Axis_t::Z_AXIS)
        {
            //TODO: check if this exepts!
            stim_instance.ensure_large_enough_for_qubits(qubit_address + 1);
            stim::GateTarget g{qubit_address};
            stim::CircuitInstruction inst{stim::GateType::S_DAG, {}, &g, ""};
            stim_instance.do_SPP_DAG(inst);
        }
        else
            return false;
    }
    else if(angle == Angle_t::_15_PI_16) //closest to PI
    {
        if(axis == Axis_t::X_AXIS)
        {
            //TODO: check if this exepts!
            stim_instance.ensure_large_enough_for_qubits(qubit_address + 1);
            stim::GateTarget g{qubit_address};
            stim::CircuitInstruction inst{stim::GateType::X, {}, &g, ""};
            stim_instance.do_X(inst);
            return true;
        }
        else if(axis == Axis_t::Y_AXIS)
        {
            //TODO: check if this exepts!
            stim_instance.ensure_large_enough_for_qubits(qubit_address + 1);
            stim::GateTarget g{qubit_address};
            stim::CircuitInstruction inst{stim::GateType::Y, {}, &g, ""};
            stim_instance.do_Y(inst);
            return true;
        }
        else if(axis == Axis_t::Z_AXIS)
        {
            //TODO: check if this exepts!
            stim_instance.ensure_large_enough_for_qubits(qubit_address + 1);
            stim::GateTarget g{qubit_address};
            stim::CircuitInstruction inst{stim::GateType::Z, {}, &g, ""};
            stim_instance.do_Z(inst);
        }
        else
            return false;
    }
    else
        return false;
}
bool StimAdapter::two_qubit_gate(uint8_t qubit1_address, uint8_t qubit2_address, Angle_t angle, Axis_t axis)
{

}
uint8_t StimAdapter::qubit_measure(uint8_t qubit_address, bool basis)
{
    stim_instance.ensure_large_enough_for_qubits(qubit_address + 1);
    stim::GateTarget g{qubit_address};
    stim::CircuitInstruction inst{stim::GateType::M, {}, &g, ""};
    stim_instance.do_MZ(inst);                                       // performs Z-measure & records bit
    return (bool)stim_instance.measurement_record.storage.back();    // return the last recorded bit
}
uint8_t StimAdapter::attempt_entanglement(uint16_t fidelity, uint8_t neighbour_address, uint8_t qid)
{

}
uint8_t StimAdapter::entanglement_measure(uint16_t fidelity, uint8_t neighbour_address, bool basis, uint8_t qid, bool try_now, uint8_t repetitions)
{

}
void StimAdapter::pre_measure_gates_selection(bool target, Angle_t angle_1, Angle_t angle_2, Angle_t angle_3)
{

}
void StimAdapter::qubit_move(uint8_t qubit1_address, uint8_t qubit2_address)
{

}
void StimAdapter::qubit_swap(uint8_t qubit1_address, uint8_t qubit2_address)
{

}
uint8_t StimAdapter::entanglement_swap(uint8_t qubit1_address, uint8_t qubit2_address)
{
    
}