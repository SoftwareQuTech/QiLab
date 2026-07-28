#include <variant>
#include <thread>

#include "simulator_manager.hpp"
#include "logger/logger_api.hpp"
#include "logger/profiler_api.hpp"
#include "Simulator/pauli_channel_math.hpp"
#include "Simulator/link_key.hpp"
#include "Simulator/NoiseModels/QubitDecoherenceNoise/depolarizing_gate_noise.hpp"


SimulatorManager::SimulatorManager(std::unique_ptr<ISimulator> simulator, bool qubit_noise_enabled, bool entanglement_noise_enabled)
    : simulator_inst(std::move(simulator)), qubit_noise_enabled(qubit_noise_enabled), entanglement_noise_enabled(entanglement_noise_enabled)
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
    return encode_link_key(node_a, node_b);
}

void SimulatorManager::configure_entanglement_links(std::unordered_map<uint16_t, std::unique_ptr<EntanglementLinkNoiseInterface>> models)
{
    for (auto &entry : models)
    {
        entanglement_link_noise_models[entry.first] = std::move(entry.second);
    }
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

    // Apply one X/Y/X sub-gate of the basis rotation. A zero angle is a no-op
    // sentinel: skip both the noise and the gate call for that sub-gate only
    // (per-gate granularity, not per-selection), matching the same skip now
    // performed uniformly in single_qubit_gate_handler/two_qubit_gate_handler.
    // Noise uses the same idle-dephasing + gate-depolarizing composition as
    // single_qubit_gate_handler, gated by the same qubit_noise_enabled flag.
    auto apply_rotation = [this, global_qubit](Angle_t angle, Axis_t axis)
    {
        if (angle == Angle_t::_0_PI_16)
        {
            return;
        }
        if (qubit_noise_enabled)
        {
            const float z_idle_noise_prob = simulation_qubits[global_qubit]->get_qubit_decoherence_probability();
            // Basis rotations are single-qubit gates, so they carry the same
            // per-qubit-array gate depolarizing probability (Sec. 2 Eq. 1).
            const float depolar_gate_prob = qubit_array_properties[global_qubit].gate_depolarizing_p;
            PauliChannelWeights composed_weights = compose_pauli_channels(0.0f, 0.0f, z_idle_noise_prob,
                                                                           depolar_gate_prob / 3.0f, depolar_gate_prob / 3.0f, depolar_gate_prob / 3.0f);
            simulator_inst->single_qubit_noise(global_qubit, composed_weights.q_x, composed_weights.q_y, composed_weights.q_z);
        }
        simulator_inst->single_qubit_gate(global_qubit, angle, axis);
    };

    apply_rotation(gates.angle1, Axis_t::X_AXIS);
    apply_rotation(gates.angle2, Axis_t::Y_AXIS);
    apply_rotation(gates.angle3, Axis_t::X_AXIS);

    // Selection persists: NOT cleared here. It re-applies on every subsequent
    // matching measurement for this node+target until overwritten by a new
    // PRE_MEASUREMENT_GATES_SELECTION (see pre_measurement_gates_selection_handler).
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
        if (cmd->angle != Angle_t::_0_PI_16)
        {
            // The single-qubit gate duration is a property of the qubit ARRAY,
            // so it is keyed by global qubit index -- resolve that first.
            info.global_qubit_1 = qubit_offset + cmd->qubit1_address;

            // This runs before the handler's own range check, and local qubit
            // addresses are 4 bits, so an out-of-range address must not index
            // qubit_array_properties. Leaving the delay at 0 is correct: the
            // handler rejects the command and no delay is applied.
            if (is_local_qubit_in_range(node_id, cmd->qubit1_address))
            {
                info.target_delay_ns =
                    qubit_array_properties[static_cast<std::size_t>(info.global_qubit_1)].single_gate_delay_ns;
            }
        }
        break;
    }
    case Command_t::TWO_QUBIT_GATE:
    {
        auto *cmd = static_cast<const TwoQubitGate *>(&command);
        if (cmd->angle != Angle_t::_0_PI_16)
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

    // Validation pass. Nothing below this point mutates manager state, so a
    // rejected node never leaves the manager half-registered.
    if (node_config.qubit_arrays.empty())
    {
        LOG_ERROR("Node '" + node_config.name + "' declares no qubit arrays.");
        return -1;
    }

    uint32_t array_qubit_sum = 0;
    for (const auto &array_config : node_config.qubit_arrays)
    {
        if (array_config.num_qubits <= 0)
        {
            LOG_ERROR("Node '" + node_config.name + "' has a qubit array with invalid size " +
                      std::to_string(array_config.num_qubits) + ". Must be > 0.");
            return -1;
        }
        array_qubit_sum += static_cast<uint32_t>(array_config.num_qubits);

        if (std::get_if<QubitNoiseAdvancedConfig>(&array_config.qubit_decoherence_noise_model.config_data))
        {
            LOG_ERROR("ADVANCED qubit noise model is not implemented; node '" + node_config.name +
                      "' not registered.");
            return -2;
        }
    }

    // The config loader derives NodeConfig::num_qubits as this sum, but
    // register_node is public API and is also driven from hand-built configs.
    if (array_qubit_sum != requested_qubits)
    {
        LOG_ERROR("Node '" + node_config.name + "' qubit arrays sum to " + std::to_string(array_qubit_sum) +
                  " qubits but the node declares " + std::to_string(requested_qubits) + ".");
        return -1;
    }

    // Commit pass. Walk the arrays with a running per-array offset so each
    // array's models and properties cover only that array's qubit range.
    uint16_t array_offset = 0;
    for (const auto &array_config : node_config.qubit_arrays)
    {
        const uint16_t array_qubits = static_cast<uint16_t>(array_config.num_qubits);
        const uint16_t array_base = static_cast<uint16_t>(registered_qubits + array_offset);
        const std::string array_range = "[" + std::to_string(array_base) + ", " +
                                        std::to_string(array_base + array_qubits - 1) + "]";

        // One single-qubit gate noise model per qubit array (Sec. 2.1). Built
        // here only to validate and log; the hot path keeps just the resulting
        // probability. Invalid parameters are rejected, not repaired: the model
        // reports p = 0 and the array runs noiseless.
        const auto &gate_noise_config = std::get<GateNoiseDepolarizingConfig>(array_config.gate_noise_model.config_data);
        const SingleQubitGateNoiseModel gate_noise_model(gate_noise_config.depolarizing_probability);
        if (!gate_noise_model.is_valid())
        {
            LOG_WARNING("Qubit array " + array_range + " of node '" + node_config.name +
                        "' has an invalid single-qubit gate depolarizing probability; "
                        "the array runs with no gate depolarizing noise.");
        }
        const float array_depolarizing_p = gate_noise_model.get_depolarizing_probability();

        // Resolve the decoherence model kind once per array rather than per qubit.
        const auto *basic_config =
            std::get_if<QubitNoiseBasicConfig>(&array_config.qubit_decoherence_noise_model.config_data);
        const auto *phase_flip_config =
            std::get_if<PhaseFlipNoiseConfig>(&array_config.qubit_decoherence_noise_model.config_data);
        if (basic_config)
        {
            LOG_INFO("Qubit array " + array_range + " uses the BASIC qubit decoherence noise model.");
        }
        else if (phase_flip_config)
        {
            LOG_INFO("Qubit array " + array_range + " uses the PHASE_FLIP qubit decoherence noise model.");
        }
        else
        {
            LOG_WARNING("Qubit array " + array_range +
                        " has no valid qubit decoherence noise model configuration. "
                        "Defaulting to BASIC model with T2=1000ns.");
        }

        for (uint16_t i = 0; i < array_qubits; i++)
        {
            const uint16_t global_qubit = static_cast<uint16_t>(array_base + i);

            if (basic_config)
            {
                simulation_qubits[global_qubit] = std::make_unique<QubitModelBasic>(*basic_config);
            }
            else if (phase_flip_config)
            {
                simulation_qubits[global_qubit] = std::make_unique<PhaseFlipQubitModel>(*phase_flip_config);
            }
            else
            {
                simulation_qubits[global_qubit] = std::make_unique<QubitModelBasic>(QubitNoiseBasicConfig{1000.0f});
            }

            qubit_array_properties[global_qubit].single_gate_delay_ns = array_config.single_gate_delay_ns;
            qubit_array_properties[global_qubit].gate_depolarizing_p = array_depolarizing_p;
        }

        array_offset = static_cast<uint16_t>(array_offset + array_qubits);
    }

    // One two-qubit gate noise model per node (Sec. 2.1). Built here only to
    // validate and log; the hot path keeps just the resulting probability.
    const auto &two_qubit_noise_config =
        std::get<GateNoiseDepolarizingConfig>(node_config.two_qubit_gate_noise_model.config_data);
    const TwoQubitGateNoiseModel two_qubit_noise_model(two_qubit_noise_config.depolarizing_probability);
    if (!two_qubit_noise_model.is_valid())
    {
        LOG_WARNING("Node '" + node_config.name +
                    "' has an invalid two-qubit gate depolarizing probability; "
                    "the node runs with no two-qubit gate depolarizing noise.");
    }
    node_two_qubit_depolarizing_p[registered_nodes] = two_qubit_noise_model.get_depolarizing_probability();

    node_operation_delays[registered_nodes] = node_config.qubit_operations_delays;


    // Assign the node ID and update the registry
    nodes[registered_nodes] = std::make_pair(registered_qubits, requested_qubits);
    // the index in the vector is the id(fast?)
    node_id = registered_nodes++;
    // Update the total count of registered qubits
    registered_qubits += requested_qubits;

    // Call the simulator to allocate qubits for this node
    // This is necessary to ensure that the STIM Tableau is aware of the new qubits and can manage their state correctly.
    simulator_inst->allocate_qubits(registered_qubits);

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

    // A zero angle is a documented no-op sentinel (Angle_t::_0_PI_16): skip both
    // the noise application and the adapter call entirely. Owning this check here
    // (rather than in the adapter) means every call site gets uniform behavior.
    if (cmd->angle == Angle_t::_0_PI_16)
    {
        response.outcome = Outcome_Status_t::SUCCESS;
        response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
        PROF_STOP(prof_event_single_qubit_gate);
        return response;
    }

    const uint16_t global_qubit = static_cast<uint16_t>(cmd->qubit1_address + qubit_offset);

    if (qubit_noise_enabled)
    {
        // Get the idling dephasing probability (between touches) for the qubit and apply it as noise before the gate operation
        const float z_idle_noise_prob = simulation_qubits[global_qubit]->get_qubit_decoherence_probability();
        // Gate depolarizing probability of the qubit's ARRAY (Sec. 2 Eq. 1).
        // Fidelity to depolar F=1-2p/3
        const float depolar_gate_prob = qubit_array_properties[global_qubit].gate_depolarizing_p;
        // combine noise and normalize noise -> returns normalized PX, PY, PZ for composed noise channel
        PauliChannelWeights composed_weights = compose_pauli_channels(0.0f, 0.0f, z_idle_noise_prob, depolar_gate_prob/3.0f, depolar_gate_prob/3.0f, depolar_gate_prob/3.0f);
        simulator_inst->single_qubit_noise(global_qubit, composed_weights.q_x, composed_weights.q_y, composed_weights.q_z);
    }

    if(!simulator_inst->single_qubit_gate(global_qubit, cmd->angle, cmd->axis))
    {
        LOG_ERROR("Single qubit gate failed");
        response.outcome = Outcome_Status_t::UNSUPPORTED_REQUEST;
        response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
        PROF_STOP(prof_event_single_qubit_gate);
        return response;
    }

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

    // A zero angle is a documented no-op sentinel (Angle_t::_0_PI_16): skip both
    // the noise application and the adapter call entirely, same as
    // single_qubit_gate_handler above.
    if (cmd->angle == Angle_t::_0_PI_16)
    {
        response.outcome = Outcome_Status_t::SUCCESS;
        response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
        PROF_STOP(prof_event_two_qubit_gate);
        return response;
    }

    const uint16_t global_qubit_1 = static_cast<uint16_t>(cmd->qubit1_address + qubit_offset);
    const uint16_t global_qubit_2 = static_cast<uint16_t>(cmd->qubit2_address + qubit_offset);

    if (qubit_noise_enabled)
    {
        // Get the dephasing probability for each qubit and apply it as noise before the gate operation
        const float z_qubit1_noise = simulation_qubits[global_qubit_1]->get_qubit_decoherence_probability();
        simulator_inst->single_qubit_noise(global_qubit_1, 0.0f, 0.0f, z_qubit1_noise);
        const float z_qubit2_noise = simulation_qubits[global_qubit_2]->get_qubit_decoherence_probability();
        simulator_inst->single_qubit_noise(global_qubit_2, 0.0f, 0.0f, z_qubit2_noise);

        // Two-qubit gate depolarizing (Sec. 2 Eq. 4), one model per NODE. The
        // channel is correlated across the pair, so it cannot be folded into
        // the two single-qubit channels above via compose_pauli_channels; it
        // goes to the backend's DEPOLARIZE2. Skipped at p == 0 to avoid a
        // pointless virtual dispatch on the hot path.
        const float depolar_gate_prob = node_two_qubit_depolarizing_p[node_id];
        if (depolar_gate_prob > 0.0f)
        {
            simulator_inst->two_qubit_depolarizing_noise(global_qubit_1, global_qubit_2, depolar_gate_prob);
        }
    }


    if (!simulator_inst->two_qubit_gate(global_qubit_1,
                                        global_qubit_2,
                                        cmd->angle,
                                        cmd->axis))
    {
        LOG_ERROR("Two-qubit gate failed between qubits " +
                  std::to_string(global_qubit_1) +
                  " and " +
                  std::to_string(global_qubit_2));
        response.outcome = Outcome_Status_t::UNSUPPORTED_REQUEST;
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

    // Entanglement-channel noise (single-qubit Pauli channel, Eq. 17) is applied
    // once per successful entanglement, only on the initiator's own qubit.
    if (entanglement_noise_enabled && is_initiator)
    {
        const uint16_t link_key = encode_pair_key(node_id, cmd->nghbr);
        const auto model_it = entanglement_link_noise_models.find(link_key);
        if (model_it != entanglement_link_noise_models.end())
        {
            const PauliChannelWeights weights = model_it->second->get_pauli_channel_weights();
            simulator_inst->single_qubit_noise(qubit_offset, weights.q_x, weights.q_y, weights.q_z);
        }
        else
        {
            LOG_WARNING("No entanglement channel model configured for link (" + std::to_string(node_id) + ", " + std::to_string(cmd->nghbr) + "); skipping noise application.");
        }
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

    // Entanglement-channel noise (single-qubit Pauli channel, Eq. 17) is applied
    // once per successful entanglement, only on the initiator's own qubit, and
    // must land before the basis rotation (step 2) and measurement (step 3) below.
    if (entanglement_noise_enabled && is_initiator)
    {
        const uint16_t link_key = encode_pair_key(node_id, cmd->nghbr);
        const auto model_it = entanglement_link_noise_models.find(link_key);
        if (model_it != entanglement_link_noise_models.end())
        {
            const PauliChannelWeights weights = model_it->second->get_pauli_channel_weights();
            simulator_inst->single_qubit_noise(local_qubit, weights.q_x, weights.q_y, weights.q_z);
        }
        else
        {
            LOG_WARNING("No entanglement channel model configured for link (" + std::to_string(node_id) + ", " + std::to_string(cmd->nghbr) + "); skipping noise application.");
        }
    }

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
        response.outcome = Outcome_Status_t::UNKNOWN_FAILURE;
        response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
        PROF_STOP(prof_event_entanglement_swap);
        return response;
    }

    response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
    response.outcome = Outcome_Status_t::SUCCESS;
    PROF_STOP(prof_event_entanglement_swap);
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

    if (qubit_noise_enabled)
    {
        // Idle dephasing only. MOVE/SWAP deliberately carry no gate depolarizing
        // noise -- the spec scopes gate depolarizing to single- and two-qubit
        // gates (Sec. 2.1), so this is intentional, not an oversight.
        float z_qubit1_noise = simulation_qubits[cmd->qubit1_address + qubit_offset]->get_qubit_decoherence_probability();
        simulator_inst->single_qubit_noise(cmd->qubit1_address + qubit_offset, 0.0f, 0.0f, z_qubit1_noise);
        float z_qubit2_noise = simulation_qubits[cmd->qubit2_address + qubit_offset]->get_qubit_decoherence_probability();
        simulator_inst->single_qubit_noise(cmd->qubit2_address + qubit_offset, 0.0f, 0.0f, z_qubit2_noise);
    }

    if (!simulator_inst->qubit_move(cmd->qubit1_address + qubit_offset,
                                    cmd->qubit2_address + qubit_offset))
    {
        LOG_ERROR("Qubit move failed from " +
                  std::to_string(cmd->qubit1_address + qubit_offset) +
                  " to " +
                  std::to_string(cmd->qubit2_address + qubit_offset));
        response.outcome = Outcome_Status_t::UNKNOWN_FAILURE;
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

    if (qubit_noise_enabled)
    {
        // Idle dephasing only. MOVE/SWAP deliberately carry no gate depolarizing
        // noise -- the spec scopes gate depolarizing to single- and two-qubit
        // gates (Sec. 2.1), so this is intentional, not an oversight.
        float z_qubit1_noise = simulation_qubits[cmd->qubit1_address + qubit_offset]->get_qubit_decoherence_probability();
        simulator_inst->single_qubit_noise(cmd->qubit1_address + qubit_offset, 0.0f, 0.0f, z_qubit1_noise);
        float z_qubit2_noise = simulation_qubits[cmd->qubit2_address + qubit_offset]->get_qubit_decoherence_probability();
        simulator_inst->single_qubit_noise(cmd->qubit2_address + qubit_offset, 0.0f, 0.0f, z_qubit2_noise);
    }

    if (!simulator_inst->qubit_swap(cmd->qubit1_address + qubit_offset,
                                    cmd->qubit2_address + qubit_offset))
    {
        LOG_ERROR("Qubit swap failed between qubits " +
                  std::to_string(cmd->qubit1_address + qubit_offset) +
                  " and " +
                  std::to_string(cmd->qubit2_address + qubit_offset));
        response.outcome = Outcome_Status_t::UNKNOWN_FAILURE;
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
   
    if (qubit_noise_enabled)
    {
        // Get the dephasing probability for the qubit and apply it as noise before the measurement operation
        float z_qubit_noise = simulation_qubits[cmd->qubit1_address + qubit_offset]->get_qubit_decoherence_probability();
        simulator_inst->single_qubit_noise(cmd->qubit1_address + qubit_offset, 0.0f, 0.0f, z_qubit_noise);
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