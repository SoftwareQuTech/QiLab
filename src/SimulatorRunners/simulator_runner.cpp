#include <memory>
#include "simulator_runner.hpp"
#include "logger/logger.hpp"
#include <iostream>
#include <chrono>
#include "logger/profiler_api.hpp"

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#define CPU_PAUSE() _mm_pause()
#elif defined(__aarch64__)
#define CPU_PAUSE() asm volatile("yield" ::: "memory")
#else
#define CPU_PAUSE() ((void)0)
#endif

SimulatorRunner::SimulatorRunner(ISimulator *sim) : simulator(sim)
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
void SimulatorRunner::set_notify_callback(std::function<void(TransportData<ResponseData>)> cb)
{
    response_notify = cb;
}

int SimulatorRunner::on_run()
{
    if (response_notify == nullptr)
    {
        std::cout << "No callback, Bye!" << std::endl;
        return -1;
    }
    while (!abort)
    {
        TransportData<CommandBase> data;
        PROF_START("simulation_thread_log");
        if (this->lockfree_queue.pop(data))
        {
            ResponseData resp;
            if (data.node_id < nodes.size())
            {
                // std::cout << "Handling command from node: " << data.node_id << '\n';
                resp = execute_command(std::move(data.transport), nodes[data.node_id].first);

                // std::this_thread::sleep_for(std::chrono::milliseconds(10)); // ok, outside lock
                //  Callbacks can safely enqueue new work now without deadlocking.
                response_notify({std::make_unique<ResponseData>(resp), data.node_id});
            }
        }
        else
        {
            CPU_PAUSE();
            // handle invalid node id, maybe set an error in resp then callback
        }
        // --- PROCESS OUTSIDE THE LOCK
        PROF_STOP("simulation_thread_log");
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
        PROF_START("command_nop");
        NOP_Command *cmd = static_cast<NOP_Command *>(command.get());
        // actually no command
        response.outcome_status = Outcome_Status_t::SUCCESS;
        PROF_STOP("command_nop");
    }
    break;
    case Command_t::QUBIT_INIT:
    {
        PROF_START("qubit_init");
        QubitInitialize *cmd = static_cast<QubitInitialize *>(command.get());
        auto res = simulator->qubit_initialize(cmd->qubit1_address + qubit_offset);
        response.outcome_status = Outcome_Status_t::SUCCESS;
        PROF_STOP("qubit_init");
    }
    break;
    case Command_t::QUBIT_MEASUREMENT:
    {
        PROF_START("qubit_measurement");
        QubitMeasure *cmd = static_cast<QubitMeasure *>(command.get());
        auto res = simulator->qubit_measure(cmd->qubit1_address + qubit_offset, cmd->basis);
        // PROF_EVENT("Measured Qubit: " + std::to_string(cmd->qubit1_address + qubit_offset) + ", value [ " + std::to_string(res) + " ]");
        response.outcome_status = Outcome_Status_t::SUCCESS;
        response.outcome_result = (uint8_t)(res == true ? (Measurement_t::MEASURE_OUTCOME_MINUS_1) : (Measurement_t::MEASURE_OUTCOME_PLUS_1));
        PROF_STOP("qubit_measurement");
    }
    break;
    case Command_t::SINGLE_QUBIT_GATE:
    {
        PROF_START("single_qubit_gate");
        SingleQubitGate *cmd = static_cast<SingleQubitGate *>(command.get());
        auto res = simulator->single_qubit_gate(cmd->qubit1_address + qubit_offset, cmd->angle, cmd->axis);
        response.outcome_status = Outcome_Status_t::SUCCESS;
        PROF_STOP("single_qubit_gate");
    }
    break;
    case Command_t::TWO_QUBIT_GATE:
    {
        PROF_START("two_qubit_gate");
        TwoQubitGate *cmd = static_cast<TwoQubitGate *>(command.get());
        auto res = simulator->two_qubit_gate(cmd->qubit1_address + qubit_offset, cmd->qubit2_address + qubit_offset, cmd->angle, cmd->axis);
        response.outcome_status = Outcome_Status_t::SUCCESS;
        PROF_STOP("two_qubit_gate");
    }
    break;
    case Command_t::QUBIT_SWAP:
    {
        PROF_START("qubit_swap");
        QubitSwap *cmd = static_cast<QubitSwap *>(command.get());
        simulator->qubit_swap(cmd->qubit1_address + qubit_offset, cmd->qubit2_address + qubit_offset);
        response.outcome_status = Outcome_Status_t::SUCCESS;
        PROF_STOP("qubit_swap");
    }
    break;
    case Command_t::ENTANGLEMENT_ATTEMPT:
    {
        PROF_START("entanglement_attempt");
        EntanglementAttempt *cmd = static_cast<EntanglementAttempt *>(command.get());
        simulator->attempt_entanglement(cmd->fidelity, cmd->nghbr + qubit_offset, cmd->qid + qubit_offset);
        response.outcome_status = Outcome_Status_t::SUCCESS;
        PROF_STOP("entanglement_attempt");
    }
    break;
    case Command_t::ENTANGLEMENT_SWAP:
    {
        PROF_START("entanglement_swap");

        EntanglementSwap *cmd = static_cast<EntanglementSwap *>(command.get());
        simulator->entanglement_swap(cmd->qubit1_address + qubit_offset, cmd->qubit2_address + qubit_offset);
        response.outcome_status = Outcome_Status_t::SUCCESS;
        PROF_STOP("entanglement_swap");
    }
    break;
    case Command_t::ENTANGLEMENT_MEASURE:
    {
        PROF_START("entanglement_measure");
        EntangleMeasure *cmd = static_cast<EntangleMeasure *>(command.get());
        simulator->entanglement_measure(cmd->fidelity, cmd->nghbr + qubit_offset, cmd->basis, cmd->qid + qubit_offset, cmd->immediate, cmd->repetitions);
        response.outcome_status = Outcome_Status_t::SUCCESS;
        PROF_STOP("entanglement_measure");
    }
    break;
    case Command_t::PRE_MEASUREMENT_GATES_SELECTION:
    {
        PROF_START("pre_measure_select");
        PreMeasureGatesSelection *cmd = static_cast<PreMeasureGatesSelection *>(command.get());
        simulator->pre_measure_gates_selection(cmd->target + qubit_offset, cmd->angle1, cmd->angle2, cmd->angle3);
        response.outcome_status = Outcome_Status_t::SUCCESS;
        PROF_STOP("pre_measure_select");
    }
    break;
    case Command_t::QUBIT_MOVE:
    {
        PROF_START("qubit_move");

        QubitMove *cmd = static_cast<QubitMove *>(command.get());
        simulator->qubit_move(cmd->qubit1_address + qubit_offset, cmd->qubit2_address + qubit_offset);
        response.outcome_status = Outcome_Status_t::SUCCESS;
        PROF_STOP("qubit_move");
    }
    break;
    default:
        break;
    }

    return response;
}
