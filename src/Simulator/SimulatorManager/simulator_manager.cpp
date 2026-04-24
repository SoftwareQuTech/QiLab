#include <variant>

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

SimulatorManager::~SimulatorManager(){}

bool SimulatorManager::is_local_qubit_in_range(uint16_t node_id, uint16_t local_qubit) const
{
    if (node_id >= registered_nodes)
    {
        return false;
    }

    const uint16_t node_qubit_count = nodes[node_id].second;
    return local_qubit < node_qubit_count;
}

bool SimulatorManager::are_local_qubits_in_range(uint16_t node_id, uint16_t local_qubit1, uint16_t local_qubit2) const
{
    return is_local_qubit_in_range(node_id, local_qubit1) &&
           is_local_qubit_in_range(node_id, local_qubit2);
}

int16_t SimulatorManager::register_node(const NodeConfig &node_config)
{
    int16_t node_id = -1;

    if (node_config.num_qubits <= 0)
    {
        LOG_ERROR("Invalid node qubit count " + std::to_string(node_config.num_qubits) + ". Must be > 0.");
        return -1;
    }

    const uint16_t requested_qubits = static_cast<uint16_t>(node_config.num_qubits);

    if (registered_nodes >= MAX_NODES_AVAILABLE)
    {
        LOG_ERROR("Maximum number of nodes reached. Cannot register more nodes.");
        return -1;
    }

    if (requested_qubits > (MAX_QUBITS_AVAILABLE - registered_qubits))
    {
        LOG_ERROR("Not enough qubit capacity to register node with " + std::to_string(requested_qubits) + " qubits.");
        return -1;
    }

    if(std::get_if<QubitNoiseBasicConfig>(&node_config.qubit_noise_model.config_data))
    {
        LOG_INFO("Registering node with BASIC qubit noise model.");
        for (uint16_t i = 0; i < requested_qubits; i++)
        {
            simulation_qubits[registered_qubits + i] = std::make_unique<QubitModelBasic>(std::get<QubitNoiseBasicConfig>(node_config.qubit_noise_model.config_data));
        }
    }
    else if(std::get_if<QubitNoiseAdvancedConfig>(&node_config.qubit_noise_model.config_data))
    {
        // Placeholder for advanced model registration
        LOG_INFO("Registering node with ADVANCED qubit noise model. NOT IMPLEMENTED");
        // simulator_manager.add_qubit_noise_model(node_id, std::make_unique<QubitModelAdvanced>(*qm_advanced));
        return -2;
    }
    else
    {
        LOG_WARNING("Node registered with no valid qubit noise model configuration. Defaulting to BASIC model with T2=1000ns.");
        for (uint16_t i = 0; i < requested_qubits; i++)
        {
            simulation_qubits[registered_qubits + i] = std::make_unique<QubitModelBasic>(QubitNoiseBasicConfig{1000.0f});
        }
    }

    // Assign the node ID and update the registry
    nodes[registered_nodes] = std::make_pair(registered_qubits, requested_qubits);
    // the index in the vector is the id(fast?)
    node_id = registered_nodes++;
    // Update the total count of registered qubits
    registered_qubits += requested_qubits;

    LOG_INFO("Registered new node " + std::to_string(node_id) + " with " + std::to_string(requested_qubits) + " qubits at offset " + std::to_string(nodes[node_id].first) + ".");
    return node_id;
}

bool SimulatorManager::unregister_node(int16_t node_id)
{
    // TODO: Add logic for deregistration and recalculating qubit offset and addresses
    // maybe qubit reset will be needed as well in order to ensure state
    LOG_WARNING("Unregistering nodes is not yet implemented.");
    return false;
}


int SimulatorManager::proccess_command(uint16_t node_id, std::unique_ptr<CommandBase> command, ResponseData& response)
{
    if (command == nullptr)
    {
        // Handle null command
        LOG_ERROR("Null command received");
        return -1;
    }
    //resolve the node and qubit offset from the command's node_id, and pass it to the handler
    if(node_id >= registered_nodes)
    {
        LOG_ERROR("Invalid node ID " + std::to_string(node_id) + " in command.");
        response.outcome = Outcome_Status_t::INVALID_QUBIT;
        response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
        return -2;
    }
    auto type = static_cast<uint8_t>(command->command_type);
    if (type >= sim_command_table.size() || sim_command_table[type] == nullptr)
    {
        // Handle invalid or unimplemented command
        LOG_ERROR("Invalid or unimplemented command received");
        return -3;
    }

    int16_t qubit_offset = nodes[node_id].first;

    response = (this->*sim_command_table[type])(std::move(command), qubit_offset, node_id);

    return 0;
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
    ResponseData response;
    auto *cmd = static_cast<SingleQubitGate *>(command.get());

    if (!is_local_qubit_in_range(node_id, cmd->qubit1_address))
    {
        LOG_ERROR("Invalid local qubit address " + std::to_string(cmd->qubit1_address) + " for node " + std::to_string(node_id) + ".");
        response.outcome = Outcome_Status_t::INVALID_QUBIT;
        response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
        return response;
    }

    if (cmd->angle == 0)
    {
        response.outcome = Outcome_Status_t::SUCCESS;
        response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
        return response;
    }
    float qubit_noise = simulation_qubits[cmd->qubit1_address + qubit_offset]->get_qubit_decoherence_probability();
    //simulator_inst->apply_qubit_noise(cmd->qubit1_address + qubit_offset, qubit_noise);
    simulator_inst->single_qubit_gate(cmd->qubit1_address + qubit_offset, cmd->angle, cmd->axis);
    simulation_qubits[cmd->qubit1_address + qubit_offset]->mark_touched();
    response.outcome = Outcome_Status_t::SUCCESS;
    response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
    return response;
}

ResponseData SimulatorManager::two_qubit_gate_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id)
{
    ResponseData response;
    auto *cmd = static_cast<TwoQubitGate *>(command.get());

    if (!are_local_qubits_in_range(node_id, cmd->qubit1_address, cmd->qubit2_address))
    {
        LOG_ERROR("Invalid local qubit address pair (" +
                  std::to_string(cmd->qubit1_address) + ", " +
                  std::to_string(cmd->qubit2_address) + ") for node " +
                  std::to_string(node_id) + ".");
        response.outcome = Outcome_Status_t::INVALID_QUBIT;
        response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
        return response;
    }

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
    ResponseData response;
    auto *cmd = static_cast<QubitInitialize *>(command.get());

    if (!is_local_qubit_in_range(node_id, cmd->qubit1_address))
    {
        LOG_ERROR("Invalid local qubit address " + std::to_string(cmd->qubit1_address) + " for node " + std::to_string(node_id) + ".");
        response.outcome = Outcome_Status_t::INVALID_QUBIT;
        response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
        return response;
    }

    simulator_inst->qubit_initialize(cmd->qubit1_address + qubit_offset);
    response.outcome = Outcome_Status_t::SUCCESS;
    response.status = NodeStatus_t::OUTCOME_READY | NodeStatus_t::READY;
    return response;
}

ResponseData SimulatorManager::entanglement_attempt_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id)
{
    ResponseData response;
    auto *cmd = static_cast<EntanglementAttempt *>(command.get());
    if (cmd->nghbr >= registered_nodes)
    {
        LOG_ERROR("Invalid neighbour index " + std::to_string(cmd->nghbr) + " for entanglement attempt.");
        response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
        response.outcome = Outcome_Status_t::INVALID_QUBIT;
        return response;
    }
    uint8_t neighbour_address = nodes[cmd->nghbr].first; // for now use only qubit address 0 of the neighbour
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
    if (cmd->nghbr >= registered_nodes)
    {
        LOG_ERROR("Invalid neighbour index " + std::to_string(cmd->nghbr) + " for entanglement attempt.");
        response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
        response.outcome = Outcome_Status_t::INVALID_QUBIT;
        return response;
    }
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
    ResponseData response;
    auto *cmd = static_cast<EntanglementSwap *>(command.get());

    if (!are_local_qubits_in_range(node_id, cmd->qubit1_address, cmd->qubit2_address))
    {
        LOG_ERROR("Invalid local qubit address pair (" +
                  std::to_string(cmd->qubit1_address) + ", " +
                  std::to_string(cmd->qubit2_address) + ") for node " +
                  std::to_string(node_id) + ".");
        response.outcome = Outcome_Status_t::INVALID_QUBIT;
        response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
        return response;
    }

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
    ResponseData response;
    auto *cmd = static_cast<QubitMove *>(command.get());

    if (!are_local_qubits_in_range(node_id, cmd->qubit1_address, cmd->qubit2_address))
    {
        LOG_ERROR("Invalid local qubit address pair (" +
                  std::to_string(cmd->qubit1_address) + ", " +
                  std::to_string(cmd->qubit2_address) + ") for node " +
                  std::to_string(node_id) + ".");
        response.outcome = Outcome_Status_t::INVALID_QUBIT;
        response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
        return response;
    }

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
    ResponseData response;
    auto *cmd = static_cast<QubitSwap *>(command.get());

    if (!are_local_qubits_in_range(node_id, cmd->qubit1_address, cmd->qubit2_address))
    {
        LOG_ERROR("Invalid local qubit address pair (" +
                  std::to_string(cmd->qubit1_address) + ", " +
                  std::to_string(cmd->qubit2_address) + ") for node " +
                  std::to_string(node_id) + ".");
        response.outcome = Outcome_Status_t::INVALID_QUBIT;
        response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
        return response;
    }

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
    ResponseData response;
    auto *cmd = static_cast<QubitMeasure *>(command.get());

    if (!is_local_qubit_in_range(node_id, cmd->qubit1_address))
    {
        LOG_ERROR("Invalid local qubit address " + std::to_string(cmd->qubit1_address) + " for node " + std::to_string(node_id) + ".");
        response.outcome = Outcome_Status_t::INVALID_QUBIT;
        response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
        return response;
    }

    auto res = simulator_inst->qubit_measure(cmd->qubit1_address + qubit_offset, cmd->basis);
    response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
    response.outcome = res ? (static_cast<uint8_t>(Measurement_t::MEASURE_OUTCOME_MINUS_1) | static_cast<uint8_t>(Outcome_Status_t::SUCCESS))
                           : (static_cast<uint8_t>(Measurement_t::MEASURE_OUTCOME_PLUS_1) | static_cast<uint8_t>(Outcome_Status_t::SUCCESS));
    return response;
}