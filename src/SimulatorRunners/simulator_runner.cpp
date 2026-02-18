#include <memory>
#include <chrono>
#include <thread>

#include "simulator_runner.hpp"
#include "utils/thread_utils.hpp"
#include "logger/logger_api.hpp"

struct Qubit
{
    //add model for decoherence, error rates, etc. if needed for more detailed simulation
    //for now, this is just a placeholder to represent qubits in the node registry
    uint8_t address; // global qubit address in the simulator
    // Optional: Add fields for tracking qubit state, entanglement metadata, etc.
};
struct NodeInfo
{
    uint16_t qubit_offset; ///< Starting address of this node's qubits in global space
    std::vector<Qubit> qubits; ///< Optional: Track individual qubit states or metadata if needed
};

SimulatorRunner::SimulatorRunner(std::unique_ptr<ISimulator> sim, std::shared_ptr<MessageBroker> broker) : simulator(std::move(sim)), message_broker(broker)
{
    PROF_THREAD_INIT();
    // PROF_EID("command_nop");
}

int16_t SimulatorRunner::register_node(uint8_t num_qubits)
{
    int16_t node_id = -1;

    if (nodes.size() >= max_nodes_available)
    {
        LOG_ERROR("Maximum number of nodes reached. Cannot register more nodes.");
        return -1;
    }

    if (registered_qubits + num_qubits > max_qubits_available)
    {
        LOG_ERROR("Not enough qubit capacity to register node with " + std::to_string(num_qubits) + " qubits.");
        return -1;
    }

    // the index in the vector is the id(fast?)
    node_id = nodes.size();

    nodes.push_back(std::make_pair(registered_qubits, num_qubits));

    registered_qubits += num_qubits;

    LOG_INFO("Registered node " + std::to_string(node_id) + " with " + std::to_string(num_qubits) + " qubits at offset " + std::to_string(nodes.back().first) + ".");
    return node_id;
}
bool SimulatorRunner::unregister_node(int16_t node_id)
{
    // TODO: Add logic for deregistration and recalculating qubit offset and addresses
    // maybe qubit reset will be needed as well in order to ensure state
    LOG_WARNING("Unregistering nodes is not yet implemented.");
    return false;
}

int SimulatorRunner::on_run(int core_id)
{
    // Set affinity at the very start
    if (core_id >= 0)
    {
        // Try to pin thread and verify success
        if (!thread_utils::pin_current_thread_to_core(core_id))
        {
            LOG_ERROR("WARNING: Failed to properly pin SimulatorRunner to core " + std::to_string(core_id) + ".");
        }

        // Double check after a small delay
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        int current_core = sched_getcpu();
        
        if (current_core != core_id)
        {
            LOG_WARNING("SimulatorRunner still not on correct core. "
                        "Expected " +
                        std::to_string(core_id) + " but on " + std::to_string(current_core) + ".");
        }
    }

    ProfGuard pg{};

    TransportData<CommandBase> command; // Command from message broker
    TransportData<ResponseData> response;

    LOG_INFO("Starting Simulator Runner with " + std::to_string(nodes.size()) + " registered nodes.");
    // Pre-initialize constant fields
    response.destination = DestinationType::CommandRunner;
    response.source = DestinationType::SimRunner;

    while (!abort)
    {
        PROF_START(prof_event_simulation_thread_log);

        if (message_broker->get_command(command, DestinationType::SimRunner))
        {
            if (command.node_id < nodes.size())
            {
                response.node_id = command.node_id;

                //LOG_INFO("Received command for node " + std::to_string(command.node_id) + " of type " + std::to_string(command.transport->command_type) + ".");
                auto resp = execute_command(std::move(command.transport), nodes[command.node_id].first, command.node_id);

                response.transport = std::make_unique<ResponseData>(std::move(resp));

                if (!message_broker->route_response(std::move(response)))
                {
                    // error
                    LOG_ERROR("Failed to route response for node " + std::to_string(command.node_id) + ".");
                }
            }
            else
            {
                LOG_ERROR("Received command for invalid node ID " + std::to_string(command.node_id) + ".");
            }
        }

        std::this_thread::sleep_for(std::chrono::microseconds(5));
        PROF_STOP(prof_event_simulation_thread_log);
        // CPU_PAUSE();
        // std::this_thread::yield();
    }
    PROF_THREAD_FLUSH();
    LOG_INFO("Ending Simulator Runner.");
    // optional: drain/notify when aborting
    return 1;
}

void SimulatorRunner::on_abort()
{
    // maybe run cleanup
    LOG_INFO("SimulatorRunner abort requested.");
    abort = true;
}

ResponseData SimulatorRunner::execute_command(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id)
{
    // PROF_START(prof_event_execute_command);

    ResponseData response;
    // Initialize common response fields

    // Cache command type before any operations that might move it
    Command_t cmd_type = command->command_type;

    switch (cmd_type)
    {
    case Command_t::NOP:
        break; // NOP requires no action

    case Command_t::QUBIT_INIT:
    {
        PROF_START(prof_event_qubit_init);
        auto *cmd = static_cast<QubitInitialize *>(command.get());
        simulator->qubit_initialize(cmd->qubit1_address + qubit_offset);
        response.outcome = Outcome_Status_t::SUCCESS;
        response.status = NodeStatus_t::OUTCOME_READY | NodeStatus_t::READY;
        PROF_STOP(prof_event_qubit_init);
    }
    break;

    case Command_t::QUBIT_MEASUREMENT:
    {
        PROF_START(prof_event_qubit_measurement);
        auto *cmd = static_cast<QubitMeasure *>(command.get());
        auto res = simulator->qubit_measure(cmd->qubit1_address + qubit_offset, cmd->basis);
        response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
        response.outcome = res ? (static_cast<uint8_t>(Measurement_t::MEASURE_OUTCOME_MINUS_1) | static_cast<uint8_t>(Outcome_Status_t::SUCCESS))
                                : (static_cast<uint8_t>(Measurement_t::MEASURE_OUTCOME_PLUS_1) | static_cast<uint8_t>(Outcome_Status_t::SUCCESS));
        LOG_INFO("Measurement result for qubit " + std::to_string(cmd->qubit1_address + qubit_offset) + ": " + (res ? "-1" : "+1"));
        PROF_STOP(prof_event_qubit_measurement);
    }
    break;

    case Command_t::SINGLE_QUBIT_GATE:
    {
        PROF_START(prof_event_single_qubit_gate);
        auto *cmd = static_cast<SingleQubitGate *>(command.get());
        simulator->single_qubit_gate(cmd->qubit1_address + qubit_offset,
                                     cmd->angle, cmd->axis);
        response.outcome = Outcome_Status_t::SUCCESS;
        response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
        PROF_STOP(prof_event_single_qubit_gate);
    }
    break;

    case Command_t::TWO_QUBIT_GATE:
    {
        PROF_START(prof_event_two_qubit_gate);
        auto *cmd = static_cast<TwoQubitGate *>(command.get());
        if(!simulator->two_qubit_gate(cmd->qubit1_address + qubit_offset, cmd->qubit2_address + qubit_offset,
                                     cmd->angle, cmd->axis))
        {
            LOG_ERROR("Two-qubit gate failed between qubits " + std::to_string(cmd->qubit1_address + qubit_offset) + " and " + std::to_string(cmd->qubit2_address + qubit_offset));
            response.outcome = Outcome_Status_t::INVALID_QUBIT;
            response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
            PROF_STOP(prof_event_two_qubit_gate);
            break;
        }
        response.outcome = Outcome_Status_t::SUCCESS;
        response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
        PROF_STOP(prof_event_two_qubit_gate);
    }
    break;

    case Command_t::QUBIT_SWAP:
    {
        PROF_START(prof_event_qubit_swap);
        auto *cmd = static_cast<QubitSwap *>(command.get());
        if (!simulator->qubit_swap(cmd->qubit1_address + qubit_offset, cmd->qubit2_address + qubit_offset))
        {
            LOG_ERROR("Qubit swap failed between qubits " + std::to_string(cmd->qubit1_address + qubit_offset) + " and " + std::to_string(cmd->qubit2_address + qubit_offset));
            response.outcome = Outcome_Status_t::INVALID_QUBIT;
            response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
            PROF_STOP(prof_event_qubit_swap);
            break;
        }
        response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
        response.outcome = Outcome_Status_t::SUCCESS;
        PROF_STOP(prof_event_qubit_swap);
    }
    break;

    case Command_t::ENTANGLEMENT_ATTEMPT:
    {
        PROF_START(prof_event_entanglement_attempt);
        auto *cmd = static_cast<EntanglementAttempt *>(command.get());
        //small amount of logic to extract the neighbour qubit address
        if(cmd->nghbr >= nodes.size())
        {
            LOG_ERROR("Invalid neighbour index " + std::to_string(cmd->nghbr) + " for entanglement attempt.");
            response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
            response.outcome = Outcome_Status_t::INVALID_QUBIT;
            PROF_STOP(prof_event_entanglement_attempt);
            break;
        }
        uint8_t neighbour_address = nodes[cmd->nghbr].first; //for now use only qubit address 0 of the neighbour
        // Determine if this node is initiator by comparing node IDs (lower ID is initiator)
        bool is_initiator = (node_id < cmd->nghbr);
        int8_t res = simulator->attempt_entanglement(cmd->fidelity,
                                        qubit_offset, //current logic is only to use qubit 0
                                        neighbour_address,
                                        is_initiator,
                                        cmd->qid);
        if(res < 0)
        {
            LOG_ERROR("Entanglement attempt failed with neighbor " + std::to_string(cmd->nghbr) + " due to simulator error.");
            response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
            response.outcome = Outcome_Status_t::ENTANGLEMENT_FAILURE;
            PROF_STOP(prof_event_entanglement_attempt);
            break;
        }
        response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
        Entaglement_t ent_state = static_cast<Entaglement_t>(res & 0x03); // Assuming the entanglement state is encoded in the lower 2 bits
        response.outcome = static_cast<int>(ent_state) | static_cast<int>(Outcome_Status_t::SUCCESS);
        LOG_INFO("Entanglement attempt result with neighbor " + std::to_string(cmd->nghbr) + ": " + std::to_string(ent_state));
        PROF_STOP(prof_event_entanglement_attempt);
    }
    break;

    case Command_t::ENTANGLEMENT_SWAP:
    {
        PROF_START(prof_event_entanglement_swap);
        auto *cmd = static_cast<EntanglementSwap *>(command.get());
        if(!simulator->entanglement_swap(cmd->qubit1_address + qubit_offset, cmd->qubit2_address + qubit_offset))
        {
            LOG_ERROR("Entanglement swap failed between qubits " + std::to_string(cmd->qubit1_address + qubit_offset) + " and " + std::to_string(cmd->qubit2_address + qubit_offset));
            response.outcome = Outcome_Status_t::INVALID_QUBIT;
            response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
            PROF_STOP(prof_event_entanglement_swap);
            break;
        }
        response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
        response.outcome = Outcome_Status_t::SUCCESS;
        PROF_STOP(prof_event_entanglement_swap);
    }
    break;

    case Command_t::ENTANGLEMENT_MEASURE:
    {
        PROF_START(prof_event_entanglement_measure);
        auto *cmd = static_cast<EntangleMeasure *>(command.get());
                //small amount of logic to extract the neighbour qubit address
        if(cmd->nghbr >= nodes.size())
        {
            LOG_ERROR("Invalid neighbour index " + std::to_string(cmd->nghbr) + " for entanglement measure attempt.");
            response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
            response.outcome = Outcome_Status_t::INVALID_QUBIT;
            PROF_STOP(prof_event_entanglement_measure);
            break;
        }
        uint8_t neighbour_address = nodes[cmd->nghbr].first; //for now use only qubit address 0 of the neighbour
        // Determine if this node is initiator by comparing node IDs (lower ID is initiator)
        bool is_initiator = (node_id < cmd->nghbr);
        int8_t res = simulator->entanglement_measure(cmd->fidelity,
                                        qubit_offset, //current logic is only to use qubit 0
                                        neighbour_address,
                                        cmd->basis,
                                        is_initiator,
                                        cmd->qid,
                                        cmd->immediate,
                                        cmd->repetitions);
        if(res < 0){
            LOG_ERROR("Entanglement measure failed with neighbor " + std::to_string(cmd->nghbr) + " due to simulator error.");
            response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
            response.outcome = Outcome_Status_t::ENTANGLEMENT_FAILURE;
            PROF_STOP(prof_event_entanglement_measure);
            break;
        }                                        
        response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
        response.outcome = res | static_cast<int>(Outcome_Status_t::SUCCESS); //the result is mesh of measure and entanglement status, so we just set the success bit and return the rest as is
        LOG_INFO("Entanglement measure result with neighbor " + std::to_string(cmd->nghbr) + ": " + std::to_string(res & 0x0F)); // Assuming the measurement outcome is encoded in the lower 4 bits
        PROF_STOP(prof_event_entanglement_measure);
    }
    break;

    case Command_t::PRE_MEASUREMENT_GATES_SELECTION:
    {
        PROF_START(prof_event_pre_measure_select);
        auto *cmd = static_cast<PreMeasureGatesSelection *>(command.get());
        simulator->pre_measure_gates_selection(cmd->target + qubit_offset,
                                               cmd->angle1,
                                               cmd->angle2,
                                               cmd->angle3);
        response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
        response.outcome = Outcome_Status_t::SUCCESS;
        PROF_STOP(prof_event_pre_measure_select);
    }
    break;

    case Command_t::QUBIT_MOVE:
    {
        PROF_START(prof_event_qubit_move);
        auto *cmd = static_cast<QubitMove *>(command.get());
        if (!simulator->qubit_move(cmd->qubit1_address + qubit_offset, cmd->qubit2_address + qubit_offset))
        {
            LOG_ERROR("Qubit move failed from " + std::to_string(cmd->qubit1_address + qubit_offset) + " to " + std::to_string(cmd->qubit2_address + qubit_offset));
            response.outcome = Outcome_Status_t::INVALID_QUBIT;
            response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
            PROF_STOP(prof_event_qubit_move);
            break;
        }
        response.outcome = Outcome_Status_t::SUCCESS;
        response.status = NodeStatus_t::READY | NodeStatus_t::OUTCOME_READY;
        PROF_STOP(prof_event_qubit_move);
    }
    break;

    default:
        response.outcome = Outcome_Status_t::UNSUPPORTED_REQUEST;
        response.status = NodeStatus_t::READY;
        break;
    }

    return response;
}
