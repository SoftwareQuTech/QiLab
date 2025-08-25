// in a source code file
#include <iostream>
#include "stim.h"
#include <random>
#include "stim/circuit/gate_target.h"
#include "stim/circuit/circuit_instruction.h"

#include <Simulator/adapters/mock_adapter.hpp>
#include <simulator_runner.hpp>
#include <CommandHandler/command_handler.hpp>
#include <CommandHandler/handlers/mock_handler.hpp>

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
    // constexpr size_t qbits = 64;
    // stim::TableauSimulator<qbits> t(std::mt19937_64(102), 25);
    // //t.do_X(1);
    // stim::GateTarget g{1};
    // t.do_MZ(stim::CircuitInstruction{stim::GateType::M, {}, &g, ""});
    // stim::Circuit c = make_bell_pair_circuit();
    // auto rec = t.measurement_record.storage.back();
    // //std::cout <<  << std::endl;
    // stim::print_circuit(std::cout, c, 0);
    MockAdapter* adapter = new MockAdapter();
    MockHandler* handler = new MockHandler();
    CommandHandler* command_handler = new CommandHandler(handler);
    SimulatorRunner sim(adapter, command_handler);

    sim.run_simulation();



    std::cout << "Executed the command" << std::endl;
    return 0;
}