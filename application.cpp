// in a source code file
#include <iostream>
#include "stim.h"

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
    stim::Circuit c = make_bell_pair_circuit();
    stim::print_circuit(std::cout, c, 0);
    std::cout << "Executed the command" << std::endl;
    return 0;
}