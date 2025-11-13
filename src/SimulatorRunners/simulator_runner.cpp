#include <memory>
#include <chrono>
#include <thread>
#include <iostream>

#include "simulator_runner.hpp"
#include "utils/thread_utils.hpp"

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
    return false;
}


int SimulatorRunner::on_run(int core_id)
{
    // Set affinity at the very start
    if (core_id >= 0) {
        // Try to pin thread and verify success
        if (!thread_utils::pin_current_thread_to_core(core_id)) {
            std::cerr << "WARNING: Failed to properly pin SimulatorRunner to core " 
                      << core_id << std::endl;
        }

        // Double check after a small delay
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        int current_core = sched_getcpu();
        if (current_core != core_id) {
            std::cerr << "WARNING: SimulatorRunner still not on correct core. "
                      << "Expected " << core_id << " but on " << current_core 
                      << std::endl;
        }
    }

    ProfGuard pg{};

    TransportData<CommandBase> command;  // Command from message broker
    TransportData<ResponseData> response;

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

                auto resp = execute_command(std::move(command.transport), nodes[command.node_id].first);
                
                response.transport = std::make_unique<ResponseData>(std::move(resp));

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
        std::this_thread::sleep_for(std::chrono::microseconds(10));
        PROF_STOP(prof_event_simulation_thread_log);
        //CPU_PAUSE();
        //std::this_thread::yield();
    }
    PROF_THREAD_FLUSH();
    // optional: drain/notify when aborting
    return 1;
}

void SimulatorRunner::on_abort()
{
    // maybe run cleanup
    abort = true;
}

ResponseData SimulatorRunner::execute_command(std::unique_ptr<CommandBase> command, int16_t qubit_offset)
{
    //PROF_START(prof_event_execute_command);
    
    ResponseData response;
    // Initialize common response fields
    response.outcome_status = Outcome_Status_t::SUCCESS;
    response.status = NodeStatus_t::BUSY_LINK;
    
    // Cache command type before any operations that might move it
    Command_t cmd_type = command->command_type;
    
    switch (cmd_type)
    {
    case Command_t::NOP:
        break;  // NOP requires no action
        
    case Command_t::QUBIT_INIT:
        {
            PROF_START(prof_event_qubit_init);
            auto* cmd = static_cast<QubitInitialize*>(command.get());
            simulator->qubit_initialize(cmd->qubit1_address + qubit_offset);
            PROF_STOP(prof_event_qubit_init);
        }
        break;
        
    case Command_t::QUBIT_MEASUREMENT:
        {
            PROF_START(prof_event_qubit_measurement);
            auto *cmd = static_cast<QubitMeasure *>(command.get());
            auto res = simulator->qubit_measure(cmd->qubit1_address + qubit_offset, cmd->basis);
            response.outcome_result = 0x11;
            response.outcome_status = Outcome_Status_t::SUCCESS;
            response.status = NodeStatus_t::BUSY_LINK;
            //std::this_thread::sleep_for(std::chrono::microseconds(1000));

            // response.outcome_result = res ? Measurement_t::MEASURE_OUTCOME_MINUS_1
            //                               : Measurement_t::MEASURE_OUTCOME_PLUS_1;
            PROF_STOP(prof_event_qubit_measurement);
        }
        break;
        
    case Command_t::SINGLE_QUBIT_GATE:
        {
            PROF_START(prof_event_single_qubit_gate);
            auto* cmd = static_cast<SingleQubitGate*>(command.get());
            simulator->single_qubit_gate(cmd->qubit1_address + qubit_offset, 
                                       cmd->angle, cmd->axis);
            response.outcome_result = 0x22;
            response.outcome_status = Outcome_Status_t::SUCCESS;
            response.status = NodeStatus_t::BUSY_LINK;

            PROF_STOP(prof_event_single_qubit_gate);
        }
        break;
        
    case Command_t::TWO_QUBIT_GATE:
        {
            PROF_START(prof_event_two_qubit_gate);
            auto* cmd = static_cast<TwoQubitGate*>(command.get());
            simulator->two_qubit_gate(cmd->qubit1_address + qubit_offset,
                                    cmd->qubit2_address + qubit_offset,
                                    cmd->angle, cmd->axis);
            PROF_STOP(prof_event_two_qubit_gate);
        }
        break;
        
    case Command_t::QUBIT_SWAP:
        {
            PROF_START(prof_event_qubit_swap);
            auto* cmd = static_cast<QubitSwap*>(command.get());
            simulator->qubit_swap(cmd->qubit1_address + qubit_offset,
                                cmd->qubit2_address + qubit_offset);
            PROF_STOP(prof_event_qubit_swap);
        }
        break;
        
    case Command_t::ENTANGLEMENT_ATTEMPT:
        {
            PROF_START(prof_event_entanglement_attempt);
            auto* cmd = static_cast<EntanglementAttempt*>(command.get());
            simulator->attempt_entanglement(cmd->fidelity, 
                                          cmd->nghbr + qubit_offset,
                                          cmd->qid + qubit_offset);
            PROF_STOP(prof_event_entanglement_attempt);
        }
        break;
        
    case Command_t::ENTANGLEMENT_SWAP:
        {
            PROF_START(prof_event_entanglement_swap);
            auto* cmd = static_cast<EntanglementSwap*>(command.get());
            simulator->entanglement_swap(cmd->qubit1_address + qubit_offset,
                                       cmd->qubit2_address + qubit_offset);
            PROF_STOP(prof_event_entanglement_swap);
        }
        break;
        
    case Command_t::ENTANGLEMENT_MEASURE:
        {
            PROF_START(prof_event_entanglement_measure);
            auto* cmd = static_cast<EntangleMeasure*>(command.get());
            simulator->entanglement_measure(cmd->fidelity,
                                          cmd->nghbr + qubit_offset,
                                          cmd->basis,
                                          cmd->qid + qubit_offset,
                                          cmd->immediate,
                                          cmd->repetitions);
            PROF_STOP(prof_event_entanglement_measure);
        }
        break;
        
    case Command_t::PRE_MEASUREMENT_GATES_SELECTION:
        {
            PROF_START(prof_event_pre_measure_select);
            auto* cmd = static_cast<PreMeasureGatesSelection*>(command.get());
            simulator->pre_measure_gates_selection(cmd->target + qubit_offset,
                                                cmd->angle1,
                                                cmd->angle2,
                                                cmd->angle3);
            PROF_STOP(prof_event_pre_measure_select);
        }
        break;
        
    case Command_t::QUBIT_MOVE:
        {
            PROF_START(prof_event_qubit_move);
            auto* cmd = static_cast<QubitMove*>(command.get());
            simulator->qubit_move(cmd->qubit1_address + qubit_offset,
                                cmd->qubit2_address + qubit_offset);
            PROF_STOP(prof_event_qubit_move);
        }
        break;
        
    default:
        response.outcome_status = Outcome_Status_t::UNSUPPORTED_REQUEST;
        break;
    }

    return response;
}
