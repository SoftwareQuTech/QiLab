#include <memory>
#include "simulator_runner.hpp"

SimulatorRunner::SimulatorRunner(ISimulator *sim, CommandHandler *handler) : simulator(sim), command_handler(handler) {}
SimulatorRunner::SimulatorRunner(ISimulator *sim, CommandHandler *handler, ILogger *logger) : simulator(sim), command_handler(handler), event_logger(logger) {}

void SimulatorRunner::run_simulation()
{
    std::unique_ptr<CommandBase> command;
    // receive some commands
    for (int i = 0; i < 240; i++)
    {
        if (event_logger != nullptr)
            event_logger->log_event("EVENT 1", "Log before command");
        if (!command_handler->receive_command(command))
        {
            if (event_logger != nullptr)
                event_logger->log_event("EVENT 2", "Log after command, before execution");
            ResponseData resp = execute_command(command.get());
            if (event_logger != nullptr)
                event_logger->log_event("EVENT 3", "Log after execution command");
            command_handler->send_response(resp);
            if (event_logger != nullptr)
                event_logger->log_event("EVENT 4", "Log after receive");
        }
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
        NOP_Command *cmd = static_cast<NOP_Command *>(command);
        // actually no command
        response.outcome_status = Outcome_Status_t::SUCCESS;
    }
    break;
    case Command_t::QUBIT_INIT:
    {
        QubitInitialize *cmd = static_cast<QubitInitialize *>(command);
        auto res = simulator->qubit_initialize(cmd->qubit1_address);
        response.outcome_status = Outcome_Status_t::SUCCESS;
    }
    break;
    case Command_t::QUBIT_MEASUREMENT:
    {
        QubitMeasure *cmd = static_cast<QubitMeasure *>(command);
        auto res = simulator->qubit_measure(cmd->qubit1_address, cmd->basis);
        if (event_logger != nullptr)
            event_logger->log_event("EVENT MEASURE", "Qubit: " + std::to_string(cmd->qubit1_address) + ", value = " + std::to_string(res));
        response.outcome_status = Outcome_Status_t::SUCCESS;
        response.outcome_result = (uint8_t)(res == true ? (Measurement_t::MEASURE_OUTCOME_MINUS_1) : (Measurement_t::MEASURE_OUTCOME_PLUS_1));
    }
    break;
    case Command_t::SINGLE_QUBIT_GATE:
    {
        SingleQubitGate *cmd = static_cast<SingleQubitGate *>(command);
        auto res = simulator->single_qubit_gate(cmd->qubit1_address, cmd->angle, cmd->axis);
        response.outcome_status = Outcome_Status_t::SUCCESS;
    }
    break;
    case Command_t::TWO_QUBIT_GATE:
    {
        TwoQubitGate *cmd = static_cast<TwoQubitGate *>(command);
        auto res = simulator->two_qubit_gate(cmd->qubit1_address, cmd->qubit2_address, cmd->angle, cmd->axis);
        response.outcome_status = Outcome_Status_t::SUCCESS;
    }
    break;
    default:
        break;
    }

    return response;
}
