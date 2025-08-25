#include <memory>
#include "simulator_runner.hpp"

SimulatorRunner::SimulatorRunner(ISimulator *sim, CommandHandler *handler) : simulator(sim), command_handler(handler) {}

void SimulatorRunner::run_simulation()
{
    std::unique_ptr<CommandBase> command;
    // receive some commands
    for (int i = 0; i < 12; i++)
    {
        command_handler->receive_command(command);
        ResponseData resp = execute_command(command.get());
        command_handler->send_response(resp);
    }
}

ResponseData SimulatorRunner::execute_command(CommandBase *command)
{
    ResponseData response;
    // MAYBE a template!
    switch (command->command_type)
    {
    case Command_t::NOP:
    {
        NOP_Command* cmd = static_cast<NOP_Command*>(command);
        //actually no command
        response.outcome = Outcome_t::SUCCESS;
    }break;
    case Command_t::QUBIT_INIT:
    {
        QubitInitialize* cmd = static_cast<QubitInitialize*>(command);
        auto res = simulator->qubit_initialize(cmd->qubit1_address);
        response.outcome = Outcome_t::SUCCESS;
    }break;
    case Command_t::QUBIT_MEASUREMENT:
    {
        QubitMeasure* cmd = static_cast<QubitMeasure*>(command);
        auto res = simulator->qubit_measure(cmd->qubit1_address, cmd->basis);
        response.outcome = Outcome_t::SUCCESS;
    }break;
    
    default:
        break;
    }

    return response;
}
