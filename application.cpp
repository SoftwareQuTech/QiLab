// in a source code file
#include <iostream>
#include "stim.h"
#include <random>

#include <Simulator/sim_interface.hpp>
#include <Simulator/adapters/mock_adapter.hpp>

stim::Circuit make_bell_pair_circuit() {
    return stim::Circuit(R"CIRCUIT(
        H 0
        CNOT 0 1
        M 0 1
        DETECTOR rec[-1] rec[-2]
    )CIRCUIT");
}

int main()
{
    constexpr size_t qbits = 64;
    stim::TableauSimulator<qbits> t(std::mt19937_64(102));
    t.read_measurement_record()
    stim::Circuit c = make_bell_pair_circuit();
    stim::print_circuit(std::cout, c, 0);
    MockAdapter* mock = new MockAdapter();
    std::cout << "Executed the command" << std::endl;
    return 0;
}