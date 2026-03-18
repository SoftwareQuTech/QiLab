#include <array>

#include "simulator_manager.hpp"
#include "logger/logger_api.hpp"
#include "logger/profiler_api.hpp"


SimulatorManager::SimulatorManager(std::unique_ptr<ISimulator> simulator)
    : simulator_inst(std::move(simulator))
{
    // Initialize the simulation command table with simulator functions.
    // for each command type, assign the corresponding handler function
    // can be done with a loop, but if the indeces change, the handlers will be broken
    sim_command_table[static_cast<uint8_t>(Command_t::NOP)] = &SimulatorManager::nop_command_handler;
    sim_command_table[static_cast<uint8_t>(Command_t::SINGLE_QUBIT_GATE)] = &SimulatorManager::single_qubit_gate_handler;
    sim_command_table[static_cast<uint8_t>(Command_t::TWO_QUBIT_GATE)] = &SimulatorManager::two_qubit_gate_handler;
    sim_command_table[static_cast<uint8_t>(Command_t::QUBIT_MEASUREMENT)] = &SimulatorManager::qubit_measure_handler;
    sim_command_table[static_cast<uint8_t>(Command_t::QUBIT_INIT)] = &SimulatorManager::qubit_init_handler;
    sim_command_table[static_cast<uint8_t>(Command_t::ENTANGLEMENT_ATTEMPT)] = &SimulatorManager::entanglement_attempt_handler;
    sim_command_table[static_cast<uint8_t>(Command_t::ENTANGLEMENT_MEASURE)] = &SimulatorManager::entanglement_measure_handler;
    sim_command_table[static_cast<uint8_t>(Command_t::ENTANGLEMENT_SWAP)] = &SimulatorManager::entanglement_swap_handler;
    sim_command_table[static_cast<uint8_t>(Command_t::PRE_MEASURE_AND_ENTANGLE_MEASURE)] = &SimulatorManager::pre_measure_and_entangle_measure_handler;
    sim_command_table[static_cast<uint8_t>(Command_t::PRE_MEASUREMENT_GATES_SELECTION)] = &SimulatorManager::pre_measurement_gates_selection_handler;
    sim_command_table[static_cast<uint8_t>(Command_t::QUBIT_MOVE)] = &SimulatorManager::qubit_move_handler;
    sim_command_table[static_cast<uint8_t>(Command_t::QUBIT_SWAP)] = &SimulatorManager::qubit_swap_handler;
}

SimulatorManager::~SimulatorManager()
{}

int SimulatorManager::add_qubits(int num_qubits, float t2_value)
{
    if (registered_qubits + num_qubits > MAX_QUBITS_AVAILABLE)
    {
        LOG_ERROR("Exceeding maximum number of qubits");
        return 0;
    }

    for (int i = 0; i < num_qubits; ++i)
    {
        simulation_qubits[registered_qubits + i] = QubitModel(t2_value);
    }
    registered_qubits += num_qubits;
    return num_qubits;
}

ResponseData SimulatorManager::proccess_command(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id)
{
    ResponseData response;
    if (command == nullptr)
    {
        // Handle null command
        LOG_ERROR("Null command received");
        return response;
    }

    auto type = static_cast<uint8_t>(command->command_type);
    if (type >= sim_command_table.size() || sim_command_table[type] == nullptr)
    {
        // Handle invalid or unimplemented command
        LOG_ERROR("Invalid or unimplemented command received");
        return response;
    }

    return (this->*sim_command_table[type])(std::move(command), qubit_offset, node_id);
}

ResponseData SimulatorManager::nop_command_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id)
{
    (void)command;
    (void)qubit_offset;
    (void)node_id;
    ResponseData response;
    response.outcome = Outcome_Status_t::SUCCESS;
    response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
    return response;
}

ResponseData SimulatorManager::single_qubit_gate_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id)
{
    (void)node_id;
    ResponseData response;
    auto *cmd = static_cast<SingleQubitGate *>(command.get());
    if (cmd->angle == 0)
    {
        response.outcome = Outcome_Status_t::SUCCESS;
        response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
        return response;
    }

    simulator_inst->single_qubit_gate(cmd->qubit1_address + qubit_offset, cmd->angle, cmd->axis);
    response.outcome = Outcome_Status_t::SUCCESS;
    response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
    return response;
}

ResponseData SimulatorManager::two_qubit_gate_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id)
{
    (void)node_id;
    ResponseData response;
    auto *cmd = static_cast<TwoQubitGate *>(command.get());
    if (cmd->angle == 0)
    {
        response.outcome = Outcome_Status_t::SUCCESS;
        response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
        return response;
    }

    if (!simulator_inst->two_qubit_gate(cmd->qubit1_address + qubit_offset,
                                        cmd->qubit2_address + qubit_offset,
                                        cmd->angle,
                                        cmd->axis))
    {
        LOG_ERROR("Two-qubit gate failed between qubits " +
                  std::to_string(cmd->qubit1_address + qubit_offset) +
                  " and " +
                  std::to_string(cmd->qubit2_address + qubit_offset));
        response.outcome = Outcome_Status_t::INVALID_QUBIT;
        response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
        return response;
    }

    response.outcome = Outcome_Status_t::SUCCESS;
    response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
    return response;
}

ResponseData SimulatorManager::qubit_init_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id)
{
    (void)node_id;
    ResponseData response;
    auto *cmd = static_cast<QubitInitialize *>(command.get());
    simulator_inst->qubit_initialize(cmd->qubit1_address + qubit_offset);
    response.outcome = Outcome_Status_t::SUCCESS;
    response.status = NodeStatus_t::OUTCOME_READY | NodeStatus_t::READY;
    return response;
}

ResponseData SimulatorManager::entanglement_attempt_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id)
{
    ResponseData response;
    auto *cmd = static_cast<EntanglementAttempt *>(command.get());
    // TODO: Replace with node-to-qubit-offset mapping once SimulatorManager owns node registry context.
    uint8_t neighbour_address = cmd->nghbr;
    bool is_initiator = (node_id < cmd->nghbr);

    int8_t res = simulator_inst->attempt_entanglement(cmd->fidelity,
                                                      qubit_offset,
                                                      neighbour_address,
                                                      is_initiator,
                                                      cmd->qid);
    if (res < 0)
    {
        LOG_ERROR("Entanglement attempt failed with neighbor " + std::to_string(cmd->nghbr) + " due to simulator error.");
        response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
        response.outcome = Outcome_Status_t::ENTANGLEMENT_FAILURE;
        return response;
    }

    response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
    Entaglement_t ent_state = static_cast<Entaglement_t>(res & 0x03);
    response.outcome = static_cast<int>(ent_state) | static_cast<int>(Outcome_Status_t::SUCCESS);
    return response;
}

ResponseData SimulatorManager::entanglement_measure_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id)
{
    ResponseData response;
    auto *cmd = static_cast<EntangleMeasure *>(command.get());
    // TODO: Replace with node-to-qubit-offset mapping once SimulatorManager owns node registry context.
    uint8_t neighbour_address = cmd->nghbr;
    bool is_initiator = (node_id < cmd->nghbr);

    int8_t res = simulator_inst->entanglement_measure(cmd->fidelity,
                                                      qubit_offset,
                                                      neighbour_address,
                                                      cmd->basis,
                                                      is_initiator,
                                                      cmd->qid,
                                                      cmd->immediate,
                                                      cmd->repetitions);
    if (res < 0)
    {
        LOG_ERROR("Entanglement measure failed with neighbor " + std::to_string(cmd->nghbr) + " due to simulator error.");
        response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
        response.outcome = Outcome_Status_t::ENTANGLEMENT_FAILURE;
        return response;
    }

    response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
    response.outcome = res | static_cast<int>(Outcome_Status_t::SUCCESS);
    return response;
}

ResponseData SimulatorManager::entanglement_swap_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id)
{
    (void)node_id;
    ResponseData response;
    auto *cmd = static_cast<EntanglementSwap *>(command.get());
    if (!simulator_inst->entanglement_swap(cmd->qubit1_address + qubit_offset,
                                           cmd->qubit2_address + qubit_offset))
    {
        LOG_ERROR("Entanglement swap failed between qubits " +
                  std::to_string(cmd->qubit1_address + qubit_offset) +
                  " and " +
                  std::to_string(cmd->qubit2_address + qubit_offset));
        response.outcome = Outcome_Status_t::INVALID_QUBIT;
        response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
        return response;
    }

    response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
    response.outcome = Outcome_Status_t::SUCCESS;
    return response;
}

ResponseData SimulatorManager::pre_measure_and_entangle_measure_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id)
{
    (void)command;
    (void)qubit_offset;
    (void)node_id;
    ResponseData response;
    response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
    response.outcome = Outcome_Status_t::UNSUPPORTED_REQUEST;
    return response;
}

ResponseData SimulatorManager::pre_measurement_gates_selection_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id)
{
    (void)node_id;
    ResponseData response;
    auto *cmd = static_cast<PreMeasureGatesSelection *>(command.get());
    simulator_inst->pre_measure_gates_selection(cmd->target + qubit_offset,
                                                cmd->angle1,
                                                cmd->angle2,
                                                cmd->angle3);
    response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
    response.outcome = Outcome_Status_t::SUCCESS;
    return response;
}

ResponseData SimulatorManager::qubit_move_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id)
{
    (void)node_id;
    ResponseData response;
    auto *cmd = static_cast<QubitMove *>(command.get());
    if (!simulator_inst->qubit_move(cmd->qubit1_address + qubit_offset,
                                    cmd->qubit2_address + qubit_offset))
    {
        LOG_ERROR("Qubit move failed from " +
                  std::to_string(cmd->qubit1_address + qubit_offset) +
                  " to " +
                  std::to_string(cmd->qubit2_address + qubit_offset));
        response.outcome = Outcome_Status_t::INVALID_QUBIT;
        response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
        return response;
    }

    response.outcome = Outcome_Status_t::SUCCESS;
    response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
    return response;
}

ResponseData SimulatorManager::qubit_swap_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id)
{
    (void)node_id;
    ResponseData response;
    auto *cmd = static_cast<QubitSwap *>(command.get());
    if (!simulator_inst->qubit_swap(cmd->qubit1_address + qubit_offset,
                                    cmd->qubit2_address + qubit_offset))
    {
        LOG_ERROR("Qubit swap failed between qubits " +
                  std::to_string(cmd->qubit1_address + qubit_offset) +
                  " and " +
                  std::to_string(cmd->qubit2_address + qubit_offset));
        response.outcome = Outcome_Status_t::INVALID_QUBIT;
        response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
        return response;
    }

    response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
    response.outcome = Outcome_Status_t::SUCCESS;
    return response;
}

ResponseData SimulatorManager::qubit_measure_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id)
{
    (void)node_id;
    ResponseData response;
    auto *cmd = static_cast<QubitMeasure *>(command.get());
    auto res = simulator_inst->qubit_measure(cmd->qubit1_address + qubit_offset, cmd->basis);
    response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
    response.outcome = res ? (static_cast<uint8_t>(Measurement_t::MEASURE_OUTCOME_MINUS_1) | static_cast<uint8_t>(Outcome_Status_t::SUCCESS))
                           : (static_cast<uint8_t>(Measurement_t::MEASURE_OUTCOME_PLUS_1) | static_cast<uint8_t>(Outcome_Status_t::SUCCESS));
    return response;
}