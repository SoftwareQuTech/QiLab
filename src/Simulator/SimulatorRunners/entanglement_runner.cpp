// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <thread>
#include <chrono>

#include "entanglement_runner.hpp"
#include "DataTypes/commands.hpp"
#include "utils/thread_utils.hpp"
#include "utils/periodic_pacer.hpp"
#include "logger/logger_api.hpp"
#include "logger/profiler_api.hpp"

EntanglementRunner::EntanglementRunner(std::shared_ptr<MessageBroker> mb,
                                       std::unordered_map<uint16_t, std::unique_ptr<EntanglementAttemptNoiseInterface>> attempt_models)
    : message_broker(mb), entanglement_manager(std::make_unique<EntanglementManager>(
                              [this](int node_id, Outcome_Status_t status)
                              {
                                  send_response_async(node_id, status);
                              },
                              [this](int node_id, std::unique_ptr<CommandBase> cmd)
                              {
                                  send_command_async(node_id, std::move(cmd));
                              }))
{
    entanglement_manager->configure_links(std::move(attempt_models));
}

int EntanglementRunner::on_run(int core_id)
{
    // Set affinity at the very start
    if (core_id >= 0)
    {
        // Try to pin thread and verify success
        if (!thread_utils::pin_current_thread_to_core(core_id))
        {
            LOG_ERROR("Failed to properly pin EntanglementRunner to core " + std::to_string(core_id) + ".");
        }

        // Double check after a small delay
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        int current_core = sched_getcpu();
        if (current_core != core_id)
        {
            LOG_WARNING("EntanglementRunner still not on correct core. Expected " + std::to_string(core_id) + " but on " + std::to_string(current_core) + ".");
        }
    }
    TransportData<CommandBase> command; // Command from message broker

    // Poll interval for the request pump. Pinned and SCHED_FIFO(80) like the other
    // runners, so this loop must block every cycle to keep the per-CPU kernel threads
    // on its core runnable.
    constexpr auto kPollInterval = std::chrono::microseconds(10);
    thread_utils::PeriodicPacer pacer{kPollInterval};

    while (!abort)
    {
        if(message_broker->get_command(command, DestinationType::EntanglementRunner))
        {
            //process command
            auto cmd = std::move(command.transport);
            uint16_t node_id = command.node_id;
            uint16_t neighbour_node = 0; // TODO: extract from cmd
            bool is_entanglement_command = true;

            //extract neighbour node from command - placeholder here
            if(cmd->command_type == Command_t::ENTANGLEMENT_ATTEMPT)
            {
                neighbour_node = static_cast<EntanglementAttempt*>(cmd.get())->nghbr; 
            }
            else if(cmd->command_type == Command_t::ENTANGLEMENT_MEASURE)
            {
                neighbour_node = static_cast<EntangleMeasure*>(cmd.get())->nghbr;
            }
            else
            {
                LOG_WARNING("Received non-entanglement command in EntanglementRunner. Ignoring.");
                is_entanglement_command = false;
            }

            // Drop an unroutable command without skipping the rest of the cycle: an
            // early `continue` here bypassed both update_requests() (delaying every
            // in-flight request) and the pacing sleep, so a run of bad commands turned
            // this into an unpaced SCHED_FIFO(80) loop.
            if(is_entanglement_command &&
               !entanglement_manager->add_new_request(std::move(cmd), node_id, neighbour_node))
            {
                LOG_ERROR("Failed to add new entanglement request from node " + std::to_string(node_id) + " to neighbour " + std::to_string(neighbour_node) + ".");
            }
        }

        entanglement_manager->update_requests();

        // Absolute-deadline pace: drift-free, and guarantees the core is released even
        // when update_requests() runs longer than the poll interval.
        pacer.wait_for_next_cycle();
    }
    PROF_THREAD_FLUSH();
    LOG_INFO("Entanglement Runner pacing: " + thread_utils::pacing_summary(pacer) + ".");
    LOG_INFO("Ending Entanglement Runner.");
    return 0;
}

void EntanglementRunner::on_abort()
{
    // maybe run cleanup
    LOG_INFO("Aborting EntanglementRunner.");
    abort = true;
}

void EntanglementRunner::send_response_async(int node_id, Outcome_Status_t status)
{
    TransportData<ResponseData> transport_data;
    transport_data.node_id = node_id;
    transport_data.destination = DestinationType::CommandRunner;
    transport_data.source = DestinationType::EntanglementRunner;
    transport_data.transport = std::make_unique<ResponseData>();
    transport_data.transport->outcome = status;
    transport_data.transport->status= NodeStatus_t::OUTCOME_READY;
    message_broker->route_response(std::move(transport_data));
}

// callback for sending command back to the node runner from manager
void EntanglementRunner::send_command_async(int node_id, std::unique_ptr<CommandBase> cmd)
{
    TransportData<CommandBase> transport_data;
    transport_data.node_id = node_id;
    transport_data.destination = DestinationType::SimRunner;
    transport_data.source = DestinationType::EntanglementRunner;
    transport_data.transport = std::move(cmd);
    message_broker->route_command(std::move(transport_data));
}