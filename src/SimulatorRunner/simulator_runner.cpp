#include <memory>
#include "simulator_runner.hpp"

SimulatorRunner::SimulatorRunner(ISimulator *sim) : simulator(sim) {}
SimulatorRunner::SimulatorRunner(ISimulator *sim, IProfiler *logger) : simulator(sim), event_logger(logger) {}

int16_t SimulatorRunner::register_node(uint8_t num_qubits)
{
    //TODO: run some verification on available qubits...
    uint16_t id;

    
}
bool SimulatorRunner::unregister_node(int16_t node_id)
{
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
        if (event_logger != nullptr)
            event_logger->pe_start("command_init");
        QubitInitialize *cmd = static_cast<QubitInitialize *>(command);
        auto res = simulator->qubit_initialize(cmd->qubit1_address);
        response.outcome_status = Outcome_Status_t::SUCCESS;
        if (event_logger != nullptr)
            event_logger->pe_stop("command_init");
    }
    break;
    case Command_t::QUBIT_MEASUREMENT:
    {
        if (event_logger != nullptr)
            event_logger->pe_start("command_measure");
        QubitMeasure *cmd = static_cast<QubitMeasure *>(command);
        auto res = simulator->qubit_measure(cmd->qubit1_address, cmd->basis);
        if (event_logger != nullptr)
            event_logger->log_event("Measured Qubit: " + std::to_string(cmd->qubit1_address) + ", value [ " + std::to_string(res) + " ]");
        response.outcome_status = Outcome_Status_t::SUCCESS;
        response.outcome_result = (uint8_t)(res == true ? (Measurement_t::MEASURE_OUTCOME_MINUS_1) : (Measurement_t::MEASURE_OUTCOME_PLUS_1));
        if (event_logger != nullptr)
            event_logger->pe_stop("command_measure");
    }
    break;
    case Command_t::SINGLE_QUBIT_GATE:
    {
        if (event_logger != nullptr)
            event_logger->pe_start("command_single_gate");
        SingleQubitGate *cmd = static_cast<SingleQubitGate *>(command);
        auto res = simulator->single_qubit_gate(cmd->qubit1_address, cmd->angle, cmd->axis);
        response.outcome_status = Outcome_Status_t::SUCCESS;
        if (event_logger != nullptr)
            event_logger->pe_stop("command_single_gate");
    }
    break;
    case Command_t::TWO_QUBIT_GATE:
    {
        if (event_logger != nullptr)
            event_logger->pe_start("command_two_q_gate");
        TwoQubitGate *cmd = static_cast<TwoQubitGate *>(command);
        auto res = simulator->two_qubit_gate(cmd->qubit1_address, cmd->qubit2_address, cmd->angle, cmd->axis);
        response.outcome_status = Outcome_Status_t::SUCCESS;
        if (event_logger != nullptr)
            event_logger->pe_stop("command_two_q_gate");
    }
    break;
    case Command_t::QUBIT_SWAP:
    {
        if (event_logger != nullptr)
            event_logger->pe_start("command_qubit_swap");
        QubitSwap *cmd = static_cast<QubitSwap *>(command);
        simulator->qubit_swap(cmd->qubit1_address, cmd->qubit2_address);
        response.outcome_status = Outcome_Status_t::SUCCESS;
        if (event_logger != nullptr)
            event_logger->pe_stop("command_qubit_swap");
    }
    break;
    case Command_t::ENTANGLEMENT_ATTEMPT:
    {
        if (event_logger != nullptr)
            event_logger->pe_start("command_ent_attempt");
        EntanglementAttempt *cmd = static_cast<EntanglementAttempt *>(command);
        simulator->attempt_entanglement(cmd->fidelity, cmd->nghbr, cmd->qid);
        response.outcome_status = Outcome_Status_t::SUCCESS;
        if (event_logger != nullptr)
            event_logger->pe_stop("command_ent_attempt");
    }
    break;
    case Command_t::ENTANGLEMENT_SWAP:
    {
        if (event_logger != nullptr)
            event_logger->pe_start("command_ent_swap");
        EntanglementSwap *cmd = static_cast<EntanglementSwap *>(command);
        simulator->entanglement_swap(cmd->qubit1_address, cmd->qubit2_address);
        response.outcome_status = Outcome_Status_t::SUCCESS;
        if (event_logger != nullptr)
            event_logger->pe_stop("command_ent_swap");
    }
    break;
    case Command_t::ENTANGLEMENT_MEASURE:
    {
        if (event_logger != nullptr)
            event_logger->pe_start("command_ent_measure");
        EntangleMeasure *cmd = static_cast<EntangleMeasure *>(command);
        simulator->entanglement_measure(cmd->fidelity, cmd->nghbr, cmd->basis, cmd->qid, cmd->immediate, cmd->repetitions);
        response.outcome_status = Outcome_Status_t::SUCCESS;
        if (event_logger != nullptr)
            event_logger->pe_stop("command_ent_measure");
    }
    break;
    case Command_t::PRE_MEASUREMENT_GATES_SELECTION:
    {
        if (event_logger != nullptr)
            event_logger->pe_start("command_pre_meas_gate");
        PreMeasureGatesSelection *cmd = static_cast<PreMeasureGatesSelection *>(command);
        simulator->pre_measure_gates_selection(cmd->target, cmd->angle1, cmd->angle2, cmd->angle3);
        response.outcome_status = Outcome_Status_t::SUCCESS;
        if (event_logger != nullptr)
            event_logger->pe_stop("command_pre_meas_gate");
    }
    break;
    case Command_t::QUBIT_MOVE:
    {
        if (event_logger != nullptr)
            event_logger->pe_start("command_qubit_move");
        QubitMove *cmd = static_cast<QubitMove *>(command);
        simulator->qubit_move(cmd->qubit1_address, cmd->qubit2_address);
        response.outcome_status = Outcome_Status_t::SUCCESS;
        if (event_logger != nullptr)
            event_logger->pe_stop("command_qubit_move");
    }
    break;
    default:
        break;
    }

    return response;
}
