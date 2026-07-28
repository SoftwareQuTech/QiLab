#include <iostream>

#include "mock_adapter.hpp"

MockAdapter::MockAdapter() {}
MockAdapter::~MockAdapter() {}

bool MockAdapter::qubit_initialize(uint8_t qubit_address)
{
    std::cout << "MOCK SIM: Initializing qubit: " << qubit_address << std::endl;
    return true;
}
bool MockAdapter::single_qubit_gate(uint8_t qubit_address, Angle_t angle, Axis_t axis)
{
    std::cout << "MOCK SIM: Single qubit gate address: " << qubit_address << std::endl;
    std::cout << "MOCK SIM: Single qubit gate angle: " << angle << std::endl;
    std::cout << "MOCK SIM: Single qubit gate axis: " << axis << std::endl;

    return true;
}
bool MockAdapter::single_qubit_noise(uint8_t qubit_address, float x_noise_probability, float y_noise_probability, float z_noise_probability)
{
    std::cout << "MOCK SIM: Apply single qubit noise on address: " << qubit_address << std::endl;
    std::cout << "MOCK SIM: Apply single qubit noise x probability: " << x_noise_probability << std::endl;
    std::cout << "MOCK SIM: Apply single qubit noise y probability: " << y_noise_probability << std::endl;
    std::cout << "MOCK SIM: Apply single qubit noise z probability: " << z_noise_probability << std::endl;

    return true;
}
bool MockAdapter::two_qubit_depolarizing_noise(uint8_t qubit1_address, uint8_t qubit2_address, float depolarizing_probability)
{
    std::cout << "MOCK SIM: Apply two qubit depolarizing noise on addresses: " << qubit1_address << ", " << qubit2_address << std::endl;
    std::cout << "MOCK SIM: Apply two qubit depolarizing noise probability: " << depolarizing_probability << std::endl;

    return true;
}
bool MockAdapter::two_qubit_gate(uint8_t qubit1_address, uint8_t qubit2_address, Angle_t angle, Axis_t axis)
{
    std::cout << "MOCK SIM: Single qubit 1 gate address: " << qubit1_address << std::endl;
    std::cout << "MOCK SIM: Single qubit 2 gate address: " << qubit2_address << std::endl;
    std::cout << "MOCK SIM: Single qubit gate angle: " << angle << std::endl;
    std::cout << "MOCK SIM: Single qubit gate axis: " << axis << std::endl;

    return true;
}
uint8_t MockAdapter::qubit_measure(uint8_t qubit_address, bool basis)
{
    std::cout << "MOCK SIM: Measure qubit with address: " << qubit_address << std::endl;
    std::cout << "MOCK SIM: Measure qubit with basis: " << basis << std::endl;

    //return some measurement result as per spec
    return 0x08;
}
int8_t MockAdapter::attempt_entanglement(uint16_t fidelity, uint8_t local_qubit, uint8_t neighbour_address, bool is_initiator, uint8_t qid)
{
    std::cout << "MOCK SIM: Attempt entanglemet with fidelity: " << fidelity << std::endl;
    std::cout << "MOCK SIM: Attempt entanglemet on local qubit: " << local_qubit << std::endl;
    std::cout << "MOCK SIM: Attempt entanglemet on neightbour: " << neighbour_address << std::endl;
    std::cout << "MOCK SIM: Attempt entanglemet with quid: " << qid << std::endl;

    return 0x09;
}
int8_t MockAdapter::entanglement_measure(uint16_t fidelity, uint8_t neighbour_address, bool basis, bool is_initiator, uint8_t qid, bool try_now, uint8_t repetitions)
{
    std::cout << "MOCK SIM: Attempt entanglemet measure with fidelity: " << fidelity << std::endl;
    std::cout << "MOCK SIM: Attempt entanglemet measure on neightbour: " << neighbour_address << std::endl;
    std::cout << "MOCK SIM: Attempt entanglemet measure with quid: " << qid << std::endl;
    std::cout << "MOCK SIM: Attempt entanglemet measure try now: " << try_now << std::endl;
    std::cout << "MOCK SIM: Attempt entanglemet measure with repetitions: " << repetitions << std::endl;

    return 0x0B;
}
void MockAdapter::pre_measure_gates_selection(bool target, Angle_t angle_1, Angle_t angle_2, Angle_t angle_3)
{
    std::cout << "MOCK SIM: Pre measure gate selection on target: " << target << std::endl;
    std::cout << "MOCK SIM: Pre measure gate selection with angle 1: " << angle_1 << std::endl;
    std::cout << "MOCK SIM: Pre measure gate selection with angle 2: " << angle_2 << std::endl;
    std::cout << "MOCK SIM: Pre measure gate selection with angle 3: " << angle_3 << std::endl;
    
}
bool MockAdapter::qubit_move(uint8_t qubit1_address, uint8_t qubit2_address)
{
    std::cout << "MOCK SIM: Qubit move address 1: " << qubit1_address << std::endl;
    std::cout << "MOCK SIM: Qubit move address 2: " << qubit2_address << std::endl;
    return true;
}
bool MockAdapter::qubit_swap(uint8_t qubit1_address, uint8_t qubit2_address)
{
    std::cout << "MOCK SIM: Qubit swap address 1: " << qubit1_address << std::endl;
    std::cout << "MOCK SIM: Qubit swap address 2: " << qubit2_address << std::endl;
    return true;
}
bool MockAdapter::entanglement_swap(uint8_t qubit1_address, uint8_t qubit2_address)
{
    std::cout << "MOCK SIM: Entanglement swap address 1: " << qubit1_address << std::endl;
    std::cout << "MOCK SIM: Entanglement swap address 2: " << qubit2_address << std::endl;

    return true;
}

bool MockAdapter::apply_qubit_noise(uint8_t qubit_address, float p_dephasing, float p_depolarizing)
{
    std::cout << "MOCK SIM: Apply qubit noise on address: " << qubit_address << std::endl;
    std::cout << "MOCK SIM: Apply qubit noise with dephasing probability: " << p_dephasing << std::endl;
    std::cout << "MOCK SIM: Apply qubit noise with depolarizing probability: " << p_depolarizing << std::endl;

    return true;
}

bool MockAdapter::allocate_qubits(uint8_t num_qubits)
{
    std::cout << "MOCK SIM: Allocate qubits: " << static_cast<int>(num_qubits) << std::endl;

    return true;
}
