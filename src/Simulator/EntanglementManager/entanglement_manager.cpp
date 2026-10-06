// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <iostream>

#include "entanglement_manager.hpp"
#include "Simulator/link_key.hpp"
#include "logger/logger_api.hpp"
#include "logger/data_acquisition_api.hpp"

#define EXPECTED_MAX_REQUESTS 100
#define EXPECTED_MAX_PAIRS 50


template <typename T>
void swap_pop_erase(std::vector<T> &vec, std::size_t idx)
{
    if (idx != vec.size() - 1)
    {
        vec[idx] = std::move(vec.back());
    }
    vec.pop_back();
}

EntanglementManager::EntanglementManager(std::function<void(int, Outcome_Status_t)> resp_cb, std::function<void(int, std::unique_ptr<CommandBase>)> cmd_cb) : response_callback(resp_cb), command_callback(cmd_cb)
{
    pending_requests.reserve(EXPECTED_MAX_REQUESTS);
    matched_pairs.reserve(EXPECTED_MAX_PAIRS);
}

bool EntanglementManager::check_request_is_expired(const EntanglementRequest &request)
{
    auto elapsed_time = std::chrono::steady_clock::now() - request.command_issue_time;
    if (elapsed_time > ENTANGLEMENT_TIMEOUT)
    {
        //LOG_INFO("Elapsed time for request from node " + std::to_string(request.node_id) + " to neighbour " + std::to_string(request.neighbour_node) + " is " + std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(elapsed_time).count()) + " microseconds, which exceeds the timeout of " + std::to_string(ENTANGLEMENT_TIMEOUT.count()) + " microseconds.");
        LOG_WARNING("Entanglement request from node " + std::to_string(request.node_id) + " to neighbour " + std::to_string(request.neighbour_node) + " has expired.");
        return true;
    }

    return false;
}

void EntanglementManager::configure_links(std::unordered_map<uint16_t, std::unique_ptr<EntanglementAttemptNoiseInterface>> models)
{
    for (auto &entry : models)
    {
        attempt_models[entry.first] = std::move(entry.second);
    }
}

bool EntanglementManager::match_request(const EntanglementRequest &request)
{
    for (auto &ent_entry : pending_requests)
    {
        if (request.neighbour_node == ent_entry.node_id)
        {
            if (request.node_id == ent_entry.neighbour_node)
            {
                return true;
            }
        }
    }
    return false;
}


int EntanglementManager::update_requests()
{
    const auto now = std::chrono::steady_clock::now();
    if (pending_requests.empty() && matched_pairs.empty())
    {
        return -1; // Nothing to process
    }
    // check pending requests
    if(pending_requests.size() > 0)
    {
        for (int i = pending_requests.size() - 1; i >= 0; --i)
        {
            if(check_request_is_expired(pending_requests[i]))
            {
                const int failed_node_id = pending_requests[i].node_id;
                //this returns data to the runner and adds the reponse to the message broker
                response_callback(failed_node_id, Outcome_Status_t::ENTANGLEMENT_SYNC_FAILURE);
                swap_pop_erase(pending_requests, static_cast<std::size_t>(i));
                LOG_INFO("Entanglement request from node " + std::to_string(failed_node_id) + " has been marked as failed due to timeout.");
            }
        }
    }

    if (matched_pairs.size() > 0)
    {
        for (int i = matched_pairs.size() - 1; i >= 0; --i)
        {
            std::chrono::duration elapsed_time = now - matched_pairs[i].match_time;
            if (elapsed_time > std::chrono::nanoseconds(matched_pairs[i].calculated_success_time_delay))
            {
                //the order of node ids must be correct in order to do the lower node ent first
                //this is the logic in the simulator runner in order to ensure only the first node does the gates and the second just measures
                if (matched_pairs[i].node_id_1 < matched_pairs[i].node_id_2) {
                    command_callback(matched_pairs[i].node_id_1, std::move(matched_pairs[i].command1));
                    command_callback(matched_pairs[i].node_id_2, std::move(matched_pairs[i].command2)); 
                }
                else
                {
                    command_callback(matched_pairs[i].node_id_2, std::move(matched_pairs[i].command2)); 
                    command_callback(matched_pairs[i].node_id_1, std::move(matched_pairs[i].command1));
                }
                LOG_INFO("Entanglement commands sent to nodes " + std::to_string(matched_pairs[i].node_id_1) + " and " + std::to_string(matched_pairs[i].node_id_2) + ".");
                swap_pop_erase(matched_pairs, static_cast<std::size_t>(i));

            }
        }
    }

    return 0;
}

bool EntanglementManager::add_new_request(std::unique_ptr<CommandBase> command, uint16_t node_id, uint16_t neighbour_node)
{
    if (node_id == neighbour_node)
    {
        // Trying to entangle with same node qubit
        LOG_ERROR("Cannot entangle qubits on a local node with entanglement command. Please use a two qubit gate instead. Node " + std::to_string(node_id) + " to neighbour " + std::to_string(neighbour_node) + " ignored.");
        return false;
    }
    // check if a request is already pending from this node to the neighbour
    for (auto it = pending_requests.begin(); it != pending_requests.end(); ++it)
    {
        if (it->neighbour_node == node_id && it->node_id == neighbour_node)
        {
            const uint16_t link_key = encode_link_key(static_cast<uint8_t>(it->node_id), static_cast<uint8_t>(node_id));
            const auto model_it = attempt_models.find(link_key);

            BatchOutcome outcome{false, 0, 0};
            if (model_it == attempt_models.end())
            {
                LOG_ERROR("No entanglement attempt model configured for link (" + std::to_string(it->node_id) + ", " + std::to_string(node_id) + "); failing fast.");
            }
            else
            {
                outcome = model_it->second->attempt_batch();
            }
            
            DAQ_RECORD("EntanglementAttempt").set("node_a", it->node_id).set("node_b", node_id).set("attempts", int(outcome.attempts)).set("duration_ns", outcome.duration_ns).set("success", outcome.success);

            if (!outcome.success)
            {
                LOG_INFO("Entanglement attempt batch failed for pair (node " + std::to_string(it->node_id) + ", node " + std::to_string(node_id) + "); reporting failure, no retry.");
                response_callback(it->node_id, Outcome_Status_t::ENTANGLEMENT_FAILURE);
                response_callback(node_id, Outcome_Status_t::ENTANGLEMENT_FAILURE);
                swap_pop_erase(pending_requests, static_cast<std::size_t>(it - pending_requests.begin()));
                return true;
            }
            matched_pairs.push_back(EntanglementPair{
                .command1 = std::move(it->command),
                .command2 = std::move(command),
                .node_id_1 = it->node_id,
                .node_id_2 = it->neighbour_node,
                .match_time = std::chrono::steady_clock::now(),
                .calculated_success_time_delay = outcome.duration_ns
            });
            swap_pop_erase(pending_requests, static_cast<std::size_t>(it - pending_requests.begin()));
            LOG_INFO("Entanglement request matched between node " + std::to_string(node_id) + " and neighbour " + std::to_string(neighbour_node) + ".");
            return true;
        }
        else if (it->node_id == node_id && it->neighbour_node == neighbour_node)
        {
            // Duplicate request from same node to same neighbour
            LOG_WARNING("Duplicate entanglement request from node " + std::to_string(node_id) + " to neighbour " + std::to_string(neighbour_node) + " ignored.");
            return false;
        }
    }

    //else we add a new pending request
    EntanglementRequest new_request;
    new_request.command = std::move(command);
    new_request.node_id = node_id;
    new_request.neighbour_node = neighbour_node;
    new_request.command_issue_time = std::chrono::steady_clock::now();

    if (pending_requests.size() == pending_requests.capacity())
    {
        LOG_WARNING("Pending entanglement request queue reached reserved capacity (" + std::to_string(pending_requests.capacity()) + "). Vector may reallocate.");
    }

    // If a matching request is not found, add it to the pending requests
    pending_requests.push_back(std::move(new_request));
    LOG_INFO("Added new entanglement request from node " + std::to_string(node_id) + " to neighbour " + std::to_string(neighbour_node) + ".");
    return true;
}
