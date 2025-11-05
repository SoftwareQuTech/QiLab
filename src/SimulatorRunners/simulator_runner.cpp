#include <memory>
#include <chrono>
#include <thread>

#include "simulator_runner.hpp"

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#define CPU_PAUSE() _mm_pause()
#elif defined(__aarch64__)
#define CPU_PAUSE() asm volatile("yield" ::: "memory")
#else
#define CPU_PAUSE() ((void)0)
#endif

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
        return -1;
    }

    if (registered_qubits + num_qubits > max_qubits_available)
    {
        return -1;
    }

    // the index in the vector is the id(fast?)
    node_id = nodes.size();

    nodes.push_back(std::make_pair(registered_qubits, num_qubits));

    registered_qubits += num_qubits;

    return node_id;
}
bool SimulatorRunner::unregister_node(int16_t node_id)
{
    // TODO: Add logic for deregistration and recalculating qubit offset and addresses
    // maybe qubit reset will be needed as well in order to ensure state
}

int SimulatorRunner::on_run()
{

    while (!abort)
    {
        TransportData<CommandBase> current_command;
        TransportData<ResponseData> response;
        //PROF_START(prof_event_simulation_thread_log);

        if (message_broker->get_command(current_command))
        {
            if (current_command.node_id < nodes.size())
            {
                response.node_id = current_command.node_id;
                response.destination = DestinationType::CommandRunner;
                response.source = DestinationType::SimRunner;

                //std::this_thread::sleep_for(std::chrono::microseconds(200));
                std::unique_ptr<ResponseData> test_resp = std::make_unique<ResponseData>();
                if (current_command.transport->command_type == Command_t::QUBIT_MEASUREMENT)
                {
                    test_resp->outcome_result = 0x11;
                    test_resp->outcome_status = Outcome_Status_t::SUCCESS;
                    test_resp->status = NodeStatus_t::BUSY_LINK;

                    response.transport = std::move(test_resp);
                }
                else if (current_command.transport->command_type == Command_t::SINGLE_QUBIT_GATE)
                {
                    test_resp->outcome_result = 0x22;
                    test_resp->outcome_status = Outcome_Status_t::SUCCESS;
                    test_resp->status = NodeStatus_t::BUSY_LINK;

                    response.transport = std::move(test_resp);
                }
                else
                {
                    response.transport = std::move(test_resp);
                }
                auto resp = execute_command(std::move(current_command.transport), nodes[current_command.node_id].first);

                // response.transport = std::make_unique<ResponseData>(resp);
                if (message_broker->route_response(std::move(response)))
                {
                    // error
                }
            }
        }
        else
        {
            // interleave a bit
            //std::this_thread::yield();
        }
        //PROF_STOP(prof_event_simulation_thread_log);
        //std::this_thread::sleep_for(std::chrono::milliseconds(1));
        std::this_thread::yield();
    }
    PROF_THREAD_FLUSH();
    // optional: drain/notify when aborting
}

void SimulatorRunner::on_abort()
{
    // maybe run cleanup
    abort = true;
}

ResponseData SimulatorRunner::execute_command(std::unique_ptr<CommandBase> command, int16_t qubit_offset)
{
    ResponseData response;
    // MAYBE a template!
    // std::cout << "Received command: [ " << command->command_type << " ] from offset: " << qubit_offset << std::endl;
    switch (command->command_type)
    {
    case Command_t::NOP:
    {
        //PROF_START(prof_event_command_nop);
        NOP_Command *cmd = static_cast<NOP_Command *>(command.get());
        // actually no command
        response.outcome_status = Outcome_Status_t::SUCCESS;
        //PROF_STOP(prof_event_command_nop);
    }
    break;
    case Command_t::QUBIT_INIT:
    {
        //PROF_START(prof_event_qubit_init);
        QubitInitialize *cmd = static_cast<QubitInitialize *>(command.get());
        auto res = simulator->qubit_initialize(cmd->qubit1_address + qubit_offset);
        response.outcome_status = Outcome_Status_t::SUCCESS;
        //PROF_STOP(prof_event_qubit_init);
    }
    break;
    case Command_t::QUBIT_MEASUREMENT:
    {
        //PROF_START(prof_event_qubit_measurement);
        QubitMeasure *cmd = static_cast<QubitMeasure *>(command.get());
        auto res = simulator->qubit_measure(cmd->qubit1_address + qubit_offset, cmd->basis);
        // PROF_EVENT("Measured Qubit: " + std::to_string(cmd->qubit1_address + qubit_offset) + ", value [ " + std::to_string(res) + " ]");
        response.outcome_status = Outcome_Status_t::SUCCESS;
        response.outcome_result = (uint8_t)(res == true ? (Measurement_t::MEASURE_OUTCOME_MINUS_1) : (Measurement_t::MEASURE_OUTCOME_PLUS_1));
        //PROF_STOP(prof_event_qubit_measurement);
    }
    break;
    case Command_t::SINGLE_QUBIT_GATE:
    {
        //PROF_START(prof_event_single_qubit_gate);
        SingleQubitGate *cmd = static_cast<SingleQubitGate *>(command.get());
        auto res = simulator->single_qubit_gate(cmd->qubit1_address + qubit_offset, cmd->angle, cmd->axis);
        response.outcome_status = Outcome_Status_t::SUCCESS;
        //PROF_STOP(prof_event_single_qubit_gate);
    }
    break;
    case Command_t::TWO_QUBIT_GATE:
    {
        //PROF_START(prof_event_two_qubit_gate);
        TwoQubitGate *cmd = static_cast<TwoQubitGate *>(command.get());
        auto res = simulator->two_qubit_gate(cmd->qubit1_address + qubit_offset, cmd->qubit2_address + qubit_offset, cmd->angle, cmd->axis);
        response.outcome_status = Outcome_Status_t::SUCCESS;
        //PROF_STOP(prof_event_two_qubit_gate);
    }
    break;
    case Command_t::QUBIT_SWAP:
    {
        //PROF_START(prof_event_qubit_swap);
        QubitSwap *cmd = static_cast<QubitSwap *>(command.get());
        simulator->qubit_swap(cmd->qubit1_address + qubit_offset, cmd->qubit2_address + qubit_offset);
        response.outcome_status = Outcome_Status_t::SUCCESS;
        //PROF_STOP(prof_event_qubit_swap);
    }
    break;
    case Command_t::ENTANGLEMENT_ATTEMPT:
    {
        //PROF_START(prof_event_entanglement_attempt);
        EntanglementAttempt *cmd = static_cast<EntanglementAttempt *>(command.get());
        simulator->attempt_entanglement(cmd->fidelity, cmd->nghbr + qubit_offset, cmd->qid + qubit_offset);
        response.outcome_status = Outcome_Status_t::SUCCESS;
        //PROF_STOP(prof_event_entanglement_attempt);
    }
    break;
    case Command_t::ENTANGLEMENT_SWAP:
    {
        //PROF_START(prof_event_entanglement_swap);

        EntanglementSwap *cmd = static_cast<EntanglementSwap *>(command.get());
        simulator->entanglement_swap(cmd->qubit1_address + qubit_offset, cmd->qubit2_address + qubit_offset);
        response.outcome_status = Outcome_Status_t::SUCCESS;
        //PROF_STOP(prof_event_entanglement_swap);
    }
    break;
    case Command_t::ENTANGLEMENT_MEASURE:
    {
        //PROF_START(prof_event_entanglement_measure);
        EntangleMeasure *cmd = static_cast<EntangleMeasure *>(command.get());
        simulator->entanglement_measure(cmd->fidelity, cmd->nghbr + qubit_offset, cmd->basis, cmd->qid + qubit_offset, cmd->immediate, cmd->repetitions);
        response.outcome_status = Outcome_Status_t::SUCCESS;
        //PROF_STOP(prof_event_entanglement_measure);
    }
    break;
    case Command_t::PRE_MEASUREMENT_GATES_SELECTION:
    {
        //PROF_START(prof_event_pre_measure_select);
        PreMeasureGatesSelection *cmd = static_cast<PreMeasureGatesSelection *>(command.get());
        simulator->pre_measure_gates_selection(cmd->target + qubit_offset, cmd->angle1, cmd->angle2, cmd->angle3);
        response.outcome_status = Outcome_Status_t::SUCCESS;
        //PROF_STOP(prof_event_pre_measure_select);
    }
    break;
    case Command_t::QUBIT_MOVE:
    {
        //PROF_START(prof_event_qubit_move);

        QubitMove *cmd = static_cast<QubitMove *>(command.get());
        simulator->qubit_move(cmd->qubit1_address + qubit_offset, cmd->qubit2_address + qubit_offset);
        response.outcome_status = Outcome_Status_t::SUCCESS;
        //PROF_STOP(prof_event_qubit_move);
    }
    break;
    default:
        break;
    }

    return response;
}
