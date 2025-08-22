#include <random>
#include "stim.h"

#include "stim_adapter.hpp"

StimAdapter::StimAdapter() 
{

}
StimAdapter::~StimAdapter() {}

bool StimAdapter::qubit_initialize(uint8_t qubit_address)
{

}
bool StimAdapter::single_qubit_gate(uint8_t qubit_address, Angle_t angle, Axis_t axis)
{

}
bool StimAdapter::two_qubit_gate(uint8_t qubit1_address, uint8_t qubit2_address, Angle_t angle, Axis_t axis)
{

}
void StimAdapter::qubit_measure(uint8_t qubit_address, bool basis)
{

}
void StimAdapter::attempt_entanglement(uint16_t fidelity, uint8_t neighbour_address, uint8_t qid)
{

}
void StimAdapter::entanglement_measure(uint16_t fidelity, uint8_t neighbour_address, bool basis, uint8_t qid, bool try_now, uint8_t repetitions)
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
void StimAdapter::entanglement_swap(uint8_t qubit1_address, uint8_t qubit2_address)
{
    
}