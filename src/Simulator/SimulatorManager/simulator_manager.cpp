#include <variant>
#include <thread>

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

uint16_t SimulatorManager::encode_pair_key(uint8_t node_a, uint8_t node_b)
{
    const uint8_t lo = (node_a < node_b) ? node_a : node_b;
    const uint8_t hi = (node_a < node_b) ? node_b : node_a;
    return (static_cast<uint16_t>(lo) << 8) | static_cast<uint16_t>(hi);
}

int8_t SimulatorManager::resolve_pair_bell_state(uint8_t node_id, uint8_t neighbour_id, bool is_initiator, int8_t local_bell)
{
    const uint16_t pair_key = encode_pair_key(node_id, neighbour_id);

    if (is_initiator)
    {
        // The initiator identifies the Bell state from the intact joint state and
        // caches it so the responder (processed next) reports the same value.
        pair_bell_states[pair_key] = local_bell;
        return local_bell;
    }

    // Responder: consume the value the initiator cached for this pair.
    auto it = pair_bell_states.find(pair_key);
    if (it != pair_bell_states.end())
    {
        const int8_t cached_bell = it->second;
        pair_bell_states.erase(it);
        return cached_bell;
    }

    LOG_WARNING("No cached Bell state for pair (" + std::to_string(node_id) + "," + std::to_string(neighbour_id) +
                "); the initiator may not have been processed first. Falling back to the local reading.");
    return local_bell;
}

void SimulatorManager::apply_pre_measure_gates(uint8_t node_id, uint16_t global_qubit, bool target)
{
    PreMeasureGates &gates = pre_measure_gates[node_id][target ? 1 : 0];
    if (!gates.enabled)
    {
        return;
    }

    LOG_INFO("Applying pre-measurement gates for node " + std::to_string(node_id) + " to Q" + std::to_string(global_qubit) +
             ": X(" + std::to_string(gates.angle1) + ") Y(" + std::to_string(gates.angle2) + ") X(" + std::to_string(gates.angle3) + ")");
    // X-Y-X rotation sequence into the requested measurement basis.
    simulator_inst->single_qubit_gate(global_qubit, gates.angle1, Axis_t::X_AXIS);
    simulator_inst->single_qubit_gate(global_qubit, gates.angle2, Axis_t::Y_AXIS);
    simulator_inst->single_qubit_gate(global_qubit, gates.angle3, Axis_t::X_AXIS);

    gates.enabled = false; // one-shot: consumed by this measurement
}

SimulatorManager::CommandTimingInfo SimulatorManager::build_command_timing_info(uint16_t node_id, int16_t qubit_offset, const CommandBase &command) const
{
    CommandTimingInfo info;

    switch (command.command_type)
    {
    case Command_t::NOP:
        info.target_delay_ns = 100;
        break;
    case Command_t::QUBIT_INIT:
    {
        auto *cmd = static_cast<const QubitInitialize *>(&command);
        info.target_delay_ns = node_operation_delays[node_id].qubit_init_delay;
        info.global_qubit_1 = qubit_offset + cmd->qubit1_address;
        break;
    }
    case Command_t::SINGLE_QUBIT_GATE:
    {
        auto *cmd = static_cast<const SingleQubitGate *>(&command);
        if (cmd->angle != 0)
        {
            info.target_delay_ns = node_operation_delays[node_id].single_gate_delay;
            info.global_qubit_1 = qubit_offset + cmd->qubit1_address;
        }
        break;
    }
    case Command_t::TWO_QUBIT_GATE:
    {
        auto *cmd = static_cast<const TwoQubitGate *>(&command);
        if (cmd->angle != 0)
        {
            info.target_delay_ns = node_operation_delays[node_id].two_gate_delay;
            info.global_qubit_1 = qubit_offset + cmd->qubit1_address;
            info.global_qubit_2 = qubit_offset + cmd->qubit2_address;
        }
        break;
    }
    case Command_t::QUBIT_MEASUREMENT:
    {
        auto *cmd = static_cast<const QubitMeasure *>(&command);
        info.target_delay_ns = node_operation_delays[node_id].measurement_delay;
        info.global_qubit_1 = qubit_offset + cmd->qubit1_address;
        break;
    }
    case Command_t::ENTANGLEMENT_ATTEMPT:
    {
        auto *cmd = static_cast<const EntanglementAttempt *>(&command);
        info.target_delay_ns = node_operation_delays[node_id].entanglement_delay;
        info.global_qubit_1 = qubit_offset + cmd->qid;
        break;
    }
    case Command_t::ENTANGLEMENT_MEASURE:
    {
        auto *cmd = static_cast<const EntangleMeasure *>(&command);
        info.target_delay_ns = node_operation_delays[node_id].entanglement_delay + node_operation_delays[node_id].measurement_delay;
        info.global_qubit_1 = qubit_offset + cmd->qid;
        break;
    }
    case Command_t::ENTANGLEMENT_SWAP:
    {
        auto *cmd = static_cast<const EntanglementSwap *>(&command);
        info.target_delay_ns = node_operation_delays[node_id].qubit_swap_delay;
        info.global_qubit_1 = qubit_offset + cmd->qubit1_address;
        info.global_qubit_2 = qubit_offset + cmd->qubit2_address;
        break;
    }
    case Command_t::QUBIT_MOVE:
    {
        auto *cmd = static_cast<const QubitMove *>(&command);
        info.target_delay_ns = node_operation_delays[node_id].qubit_move_delay;
        info.global_qubit_1 = qubit_offset + cmd->qubit1_address;
        info.global_qubit_2 = qubit_offset + cmd->qubit2_address;
        break;
    }
    case Command_t::QUBIT_SWAP:
    {
        auto *cmd = static_cast<const QubitSwap *>(&command);
        info.target_delay_ns = node_operation_delays[node_id].qubit_swap_delay;
        info.global_qubit_1 = qubit_offset + cmd->qubit1_address;
        info.global_qubit_2 = qubit_offset + cmd->qubit2_address;
        break;
    }
    default:
        break;
    }

    return info;
}

void SimulatorManager::apply_touch_with_delay(const CommandTimingInfo &timing_info, uint64_t wait_ns)
{
    if (timing_info.global_qubit_1 >= 0)
    {
        simulation_qubits[static_cast<std::size_t>(timing_info.global_qubit_1)]->mark_touched(static_cast<int64_t>(wait_ns));
    }
    if (timing_info.global_qubit_2 >= 0)
    {
        simulation_qubits[static_cast<std::size_t>(timing_info.global_qubit_2)]->mark_touched(static_cast<int64_t>(wait_ns));
    }
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

    for (const auto &array_config : node_config.qubit_arrays)
    {
        if (std::get_if<QubitNoiseBasicConfig>(&array_config.noise_model.config_data))
        {
            LOG_INFO("Registering node with BASIC qubit noise model.");
            for (uint16_t i = 0; i < requested_qubits; i++)
            {
                simulation_qubits[registered_qubits + i] = std::make_unique<QubitModelBasic>(std::get<QubitNoiseBasicConfig>(array_config.noise_model.config_data));
            }
        }
        else if (std::get_if<QubitNoiseAdvancedConfig>(&array_config.noise_model.config_data))
        {
            // Placeholder for advanced model registration
            LOG_INFO("Registering node with ADVANCED qubit noise model. NOT IMPLEMENTED");
            // simulator_manager.add_qubit_noise_model(node_id, std::make_unique<QubitModelAdvanced>(*qm_advanced));
            return -2;
        }
        else if (std::get_if<PhaseFlipNoiseConfig>(&array_config.noise_model.config_data))
        {
            // Placeholder for phase flip model registration
            LOG_INFO("Registering node with PHASE_FLIP qubit noise model.");
            for (uint16_t i = 0; i < requested_qubits; i++)
            {
                simulation_qubits[registered_qubits + i] = std::make_unique<PhaseFlipQubitModel>(std::get<PhaseFlipNoiseConfig>(array_config.noise_model.config_data));
        }
        }
        else
        {
            LOG_WARNING("Node registered with no valid qubit noise model configuration. Defaulting to BASIC model with T2=1000ns.");
            for (uint16_t i = 0; i < requested_qubits; i++)
            {
                simulation_qubits[registered_qubits + i] = std::make_unique<QubitModelBasic>(QubitNoiseBasicConfig{1000.0f});
            }
        }
    }

    // Assign the noises to the node noise array
    node_operation_delays[registered_nodes].single_gate_delay = node_config.qubit_operations_delays.single_gate_delay;
    node_operation_delays[registered_nodes].two_gate_delay = node_config.qubit_operations_delays.two_gate_delay;
    node_operation_delays[registered_nodes].entanglement_delay = node_config.qubit_operations_delays.entanglement_delay;
    node_operation_delays[registered_nodes].measurement_delay = node_config.qubit_operations_delays.measurement_delay;
    node_operation_delays[registered_nodes].qubit_swap_delay = node_config.qubit_operations_delays.qubit_swap_delay;
    node_operation_delays[registered_nodes].qubit_move_delay = node_config.qubit_operations_delays.qubit_move_delay;
    node_operation_delays[registered_nodes].qubit_init_delay = node_config.qubit_operations_delays.qubit_init_delay;


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


int SimulatorManager::add_new_command_to_process(uint16_t node_id, std::unique_ptr<CommandBase> command, ResponseData& response)
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
    const CommandTimingInfo timing_info = build_command_timing_info(node_id, qubit_offset, *command);

    const auto start_time = clock_type::now();
    response = (this->*sim_command_table[type])(std::move(command), qubit_offset, node_id);
    const auto elapsed_ns = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(clock_type::now() - start_time).count());

    uint64_t wait_ns = 0;
    const bool success = (response.outcome & static_cast<uint8_t>(Outcome_Status_t::SUCCESS)) != 0;
    if (success)
    {
        // LOG_INFO("Command for node " + std::to_string(node_id) + " executed successfully in " + std::to_string(elapsed_ns) + " ns. Target delay was " + std::to_string(timing_info.target_delay_ns) + " ns.");
        wait_ns = (timing_info.target_delay_ns > elapsed_ns) ? (timing_info.target_delay_ns - elapsed_ns) : 0;
        apply_touch_with_delay(timing_info, wait_ns);
    }

    operations_engine.add_new_response_to_wait(response, node_id, wait_ns);

    return 0;
}

int SimulatorManager::try_get_ready_response(ResponseData& response, uint16_t &node_id)
{
    int ret_code = -1;
    //must apply touched paradigm
    if(operations_engine.try_get_ready_response(response, node_id))
    {
        //mark the qubit as touched in order to update the noise model
        // this is needed in order to ensure the noise is updated after the delay has passed, and not before, which would be unrealistic
        // the touched paradigm is that the noise is updated only when the qubit is accessed, and not when the command is executed, in order to ensure the noise is updated after the delay has passed, and not before, which would be unrealistic
        // this is especially important for the single qubit gate, which can have a long delay, and the noise should be updated after the delay
        ret_code = 0;
    }

    return ret_code;
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
    PROF_START(prof_event_single_qubit_gate);
    ResponseData response;
    auto *cmd = static_cast<SingleQubitGate *>(command.get());

    if (!is_local_qubit_in_range(node_id, cmd->qubit1_address))
    {
        LOG_ERROR("Invalid local qubit address " + std::to_string(cmd->qubit1_address) + " for node " + std::to_string(node_id) + ".");
        response.outcome = Outcome_Status_t::INVALID_QUBIT;
        response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
        PROF_STOP(prof_event_single_qubit_gate);
        return response;
    }

    if (cmd->angle == 0)
    {
        response.outcome = Outcome_Status_t::SUCCESS;
        response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
        PROF_STOP(prof_event_single_qubit_gate);
        return response;
    }

    /// Apply the operations noises here, before the operation is executed, in order to ensure the noise is applied even if the operation fails due to invalid qubits or other reasons, which would be more realistic
    // Get the qubit in memory noise which shold be dephasing
    auto p_dephasing = simulation_qubits[cmd->qubit1_address + qubit_offset]->get_qubit_decoherence_probability();
    // Get the qubit noise for the single gate operation, which should be depolarizing
    //auto p_dephasing = simulation_gates->get_single_qubit_gate_noise_probability(angle, axis);
    // Combine the noises to get the total noise probability for the operation, this is a simple model, in reality the noise would be more complex and depend on the specific operation and the qubit state, but this is a good starting point
    //auto total_noise_probability = 1 - (1 - p_dephasing) * (1 - p_single_gate);
    // Apply the noise
    //simualtor_inst->apply_qubit_noise(cmd->qubit1_address + qubit_offset, p_dephasing, p_polarizing);

    time_point start_time = clock_type::now();
    simulator_inst->single_qubit_gate(cmd->qubit1_address + qubit_offset, cmd->angle, cmd->axis);
    response.outcome = Outcome_Status_t::SUCCESS;
    response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
    PROF_STOP(prof_event_single_qubit_gate);
    return response;
}

ResponseData SimulatorManager::two_qubit_gate_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id)
{
    PROF_START(prof_event_two_qubit_gate);
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
PROF_STOP(prof_event_two_qubit_gate);
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
        PROF_STOP(prof_event_two_qubit_gate);
        return response;
    }

    response.outcome = Outcome_Status_t::SUCCESS;
    response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
    PROF_STOP(prof_event_two_qubit_gate);
    return response;
}

ResponseData SimulatorManager::qubit_init_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id)
{
    PROF_START(prof_event_qubit_init);
    ResponseData response;
    auto *cmd = static_cast<QubitInitialize *>(command.get());

    if (!is_local_qubit_in_range(node_id, cmd->qubit1_address))
    {
        LOG_ERROR("Invalid local qubit address " + std::to_string(cmd->qubit1_address) + " for node " + std::to_string(node_id) + ".");
        response.outcome = Outcome_Status_t::INVALID_QUBIT;
        response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
        PROF_STOP(prof_event_qubit_init);
        return response;
    }

    simulator_inst->qubit_initialize(cmd->qubit1_address + qubit_offset);
    response.outcome = Outcome_Status_t::SUCCESS;
    response.status = NodeStatus_t::OUTCOME_READY | NodeStatus_t::READY;
    PROF_STOP(prof_event_qubit_init);
    return response;
}

ResponseData SimulatorManager::entanglement_attempt_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id)
{
    PROF_START(prof_event_entanglement_attempt);
    ResponseData response;
    auto *cmd = static_cast<EntanglementAttempt *>(command.get());
    if (cmd->nghbr >= registered_nodes)
    {
        LOG_ERROR("Invalid neighbour index " + std::to_string(cmd->nghbr) + " for entanglement attempt.");
        response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
        response.outcome = Outcome_Status_t::INVALID_QUBIT;
        PROF_STOP(prof_event_entanglement_attempt);
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
        PROF_STOP(prof_event_entanglement_attempt);
        return response;
    }

    // Both nodes of the pair must report the same 4-way Bell state. Only the
    // initiator can read it from the intact joint state; the responder gets it
    // from the manager's per-pair cache.
    const int8_t bell_state = resolve_pair_bell_state(node_id, cmd->nghbr, is_initiator, res & 0x03);

    response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
    Entaglement_t ent_state = static_cast<Entaglement_t>(bell_state & 0x03);
    response.outcome = static_cast<int>(ent_state) | static_cast<int>(Outcome_Status_t::SUCCESS);
    PROF_STOP(prof_event_entanglement_attempt);
    return response;
}

ResponseData SimulatorManager::entanglement_measure_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id)
{
    PROF_START(prof_event_entanglement_measure);
    ResponseData response;
    auto *cmd = static_cast<EntangleMeasure *>(command.get());
    if (cmd->nghbr >= registered_nodes)
    {
        LOG_ERROR("Invalid neighbour index " + std::to_string(cmd->nghbr) + " for entanglement attempt.");
        response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
        response.outcome = Outcome_Status_t::INVALID_QUBIT;
        PROF_STOP(prof_event_entanglement_measure);
        return response;
    }
    // Resolve the neighbour's global qubit address (qubit 0 of the neighbour node
    // for now), mirroring entanglement_attempt_handler. Using the raw node id here
    // only worked when node_id == qubit index (one qubit per node); resolving it
    // through the node registry is what makes this scale to multiple qubits/nodes.
    uint8_t neighbour_address = nodes[cmd->nghbr].first;
    bool is_initiator = (node_id < cmd->nghbr);

    // Entanglement measurement is orchestrated here, in one atomic handler call,
    // instead of a single adapter method. This decouples the steps and keeps the
    // adapter free of pre-measurement state:
    //   1) establish + identify the Bell state (initiator builds the pair and
    //      peeks it from the intact joint state; responder is a no-op),
    //   2) apply this node's basis-rotation gates to its own qubit (target=1),
    //   3) measure this node's own qubit.
    const uint16_t local_qubit = static_cast<uint16_t>(qubit_offset);

    // Step 1: establish + identify. attempt_entanglement is the shared primitive
    // (H+CNOT + Bell-state peek for the initiator; no-op for the responder).
    int8_t est = simulator_inst->attempt_entanglement(cmd->fidelity,
                                                      local_qubit,
                                                      neighbour_address,
                                                      is_initiator,
                                                      cmd->qid);
    if (est < 0)
    {
        LOG_ERROR("Entanglement measure failed with neighbor " + std::to_string(cmd->nghbr) + " due to simulator error.");
        response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
        response.outcome = Outcome_Status_t::ENTANGLEMENT_FAILURE;
        PROF_STOP(prof_event_entanglement_measure);
        return response;
    }

    // Both nodes must report the same 4-way Bell state. The initiator caches its
    // pre-collapse reading; the responder reads it back from the manager cache.
    const int8_t bell_state = resolve_pair_bell_state(node_id, cmd->nghbr, is_initiator, est & 0x03);

    // Step 2: basis rotation on this node's own qubit (must land AFTER the peek in
    // step 1 and BEFORE the measurement in step 3 -- which is exactly this order).
    apply_pre_measure_gates(node_id, local_qubit, /*target=*/true);

    // Step 3: measure this node's own qubit for its (correlated) outcome bit.
    const uint8_t measurement = simulator_inst->qubit_measure(local_qubit, cmd->basis);

    response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
    response.outcome = (bell_state & 0x03) | ((measurement & 0x01) << 2) | static_cast<int>(Outcome_Status_t::SUCCESS);
    PROF_STOP(prof_event_entanglement_measure);
    return response;
}

ResponseData SimulatorManager::entanglement_swap_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id)
{
    PROF_START(prof_event_entanglement_swap);
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
        PROF_STOP(prof_event_entanglement_swap);
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
        PROF_STOP(prof_event_entanglement_swap);
        return response;
    }

    response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
    response.outcome = Outcome_Status_t::SUCCESS;
    PROF_STOP(prof_event_entanglement_swap);
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
    PROF_START(prof_event_pre_measure_select);
    (void)qubit_offset;
    ResponseData response;
    auto *cmd = static_cast<PreMeasureGatesSelection *>(command.get());

    // Store the selection per node (and per target), NOT in the adapter. The next
    // matching measurement for THIS node applies and consumes it. This prevents
    // cross-node contamination: previously a single adapter-global latch could be
    // consumed by whichever node measured next.
    // (cmd->target: 0 = regular qubit_measure, 1 = entanglement measure.)
    PreMeasureGates &gates = pre_measure_gates[node_id][cmd->target ? 1 : 0];
    gates.enabled = true;
    gates.angle1 = cmd->angle1;
    gates.angle2 = cmd->angle2;
    gates.angle3 = cmd->angle3;

    LOG_INFO("Stored pre-measurement gates for node " + std::to_string(node_id) + " (target=" + std::to_string(cmd->target) +
             "): angle1=" + std::to_string(cmd->angle1) + " angle2=" + std::to_string(cmd->angle2) + " angle3=" + std::to_string(cmd->angle3));

    response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
    response.outcome = Outcome_Status_t::SUCCESS;
    PROF_STOP(prof_event_pre_measure_select);
    return response;
}

ResponseData SimulatorManager::qubit_move_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id)
{
    PROF_START(prof_event_qubit_move);
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
        PROF_STOP(prof_event_qubit_move);
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
        PROF_STOP(prof_event_qubit_move);
        return response;
    }

    response.outcome = Outcome_Status_t::SUCCESS;
    response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
    PROF_STOP(prof_event_qubit_move);
    return response;
}

ResponseData SimulatorManager::qubit_swap_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id)
{
    PROF_START(prof_event_qubit_swap);
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
        PROF_STOP(prof_event_qubit_swap);
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
        PROF_STOP(prof_event_qubit_swap);
        return response;
    }

    response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
    response.outcome = Outcome_Status_t::SUCCESS;
    PROF_STOP(prof_event_qubit_swap);
    return response;
}

ResponseData SimulatorManager::qubit_measure_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id)
{
    PROF_START(prof_event_qubit_measurement);
    ResponseData response;
    auto *cmd = static_cast<QubitMeasure *>(command.get());

    if (!is_local_qubit_in_range(node_id, cmd->qubit1_address))
    {
        LOG_ERROR("Invalid local qubit address " + std::to_string(cmd->qubit1_address) + " for node " + std::to_string(node_id) + ".");
        response.outcome = Outcome_Status_t::INVALID_QUBIT;
        response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
        PROF_STOP(prof_event_qubit_measurement);
        return response;
    }

    // Apply this node's pending basis-rotation gates (target=0) to its own qubit
    // immediately before measuring, within this single atomic handler call, so no
    // other node's command can consume them first.
    const uint16_t global_qubit = static_cast<uint16_t>(cmd->qubit1_address + qubit_offset);
    apply_pre_measure_gates(node_id, global_qubit, /*target=*/false);

    auto res = simulator_inst->qubit_measure(global_qubit, cmd->basis);
    response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
    response.outcome = res ? (static_cast<uint8_t>(Measurement_t::MEASURE_OUTCOME_MINUS_1) | static_cast<uint8_t>(Outcome_Status_t::SUCCESS))
                           : (static_cast<uint8_t>(Measurement_t::MEASURE_OUTCOME_PLUS_1) | static_cast<uint8_t>(Outcome_Status_t::SUCCESS));
    PROF_STOP(prof_event_qubit_measurement);
    return response;
}