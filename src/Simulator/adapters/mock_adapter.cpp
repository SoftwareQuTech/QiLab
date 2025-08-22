#include "mock_adapter.hpp"

MockAdapter::MockAdapter() {}
MockAdapter::~MockAdapter() {}

bool MockAdapter::qubit_initialize(uint8_t qubit_address)
{

}
bool MockAdapter::single_qubit_gate(uint8_t qubit_address, Angle_t angle, Axis_t axis)
{

}
bool MockAdapter::two_qubit_gate(uint8_t qubit1_address, uint8_t qubit2_address, Angle_t angle, Axis_t axis)
{

}
void MockAdapter::qubit_measure(uint8_t qubit_address, bool basis)
{

}
void MockAdapter::attempt_entanglement(uint16_t fidelity, uint8_t neighbour_address, uint8_t qid)
{

}
void MockAdapter::entanglement_measure(uint16_t fidelity, uint8_t neighbour_address, bool basis, uint8_t qid, bool try_now, uint8_t repetitions)
{

}
void MockAdapter::pre_measure_gates_selection(bool target, Angle_t angle_1, Angle_t angle_2, Angle_t angle_3)
{

}
void MockAdapter::qubit_move(uint8_t qubit1_address, uint8_t qubit2_address)
{

}
void MockAdapter::qubit_swap(uint8_t qubit1_address, uint8_t qubit2_address)
{

}
void MockAdapter::entanglement_swap(uint8_t qubit1_address, uint8_t qubit2_address)
{
    
}