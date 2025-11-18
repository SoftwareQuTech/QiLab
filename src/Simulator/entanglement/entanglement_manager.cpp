#include <iostream>

#include "Simulator/entanglement/entanglement_manager.hpp"
#include "logger/logger_api.hpp"

#define EXPECTED_MAX_REQUESTS 100
#define EXPECTED_MAX_PAIRS 50

EntanglementManager::EntanglementManager(std::function<void(int, Outcome_Status_t)> resp_cb, std::function<void(int, std::unique_ptr<CommandBase>)> cmd_cb) : response_callback(resp_cb), command_callback(cmd_cb)
{
    pending_requests.reserve(EXPECTED_MAX_REQUESTS);
    matched_pairs.reserve(EXPECTED_MAX_PAIRS);
}

bool EntanglementManager::check_request_is_expired(const EntanglementRequest &request)
{
    std::chrono::duration elapsed_time = std::chrono::steady_clock::now() - request.command_issue_time;
    if (std::chrono::microseconds(elapsed_time.count()) > ENTANGLEMENT_TIMEOUT)
    {
        LOG_WARNING("Entanglement request from node " + std::to_string(request.node_id) + " to neighbour " + std::to_string(request.neighbour_node) + " has expired.");
        return true;
    }

    return false;
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
                //this returns data to the runner and adds the reponse to the message broker
                response_callback(pending_requests[i].node_id, Outcome_Status_t::ENTANGLEMENT_SYNC_FAILURE);
                pending_requests.erase(pending_requests.begin() + i);
                LOG_INFO("Entanglement request from node " + std::to_string(pending_requests[i].node_id) + " has been marked as failed due to timeout.");
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
                command_callback(matched_pairs[i].node_id_1, std::move(matched_pairs[i].command1));
                command_callback(matched_pairs[i].node_id_2, std::move(matched_pairs[i].command2));
                matched_pairs.erase(matched_pairs.begin() + i);
                LOG_INFO("Entanglement commands sent to nodes " + std::to_string(matched_pairs[i].node_id_1) + " and " + std::to_string(matched_pairs[i].node_id_2) + ".");
            }
        }
    }

    return 0;
}

bool EntanglementManager::add_new_request(std::unique_ptr<CommandBase> command, uint16_t node_id, uint16_t neighbour_node)
{
    //check if a request is already pending from this node to the neighbour
    for (auto it = pending_requests.begin(); it != pending_requests.end(); ++it) {
        if (it->neighbour_node == node_id && it->node_id == neighbour_node) {
            matched_pairs.push_back(EntanglementPair{
                .command1 = std::move(it->command),
                .command2 = std::move(command),
                .node_id_1 = it->node_id,
                .node_id_2 = it->neighbour_node,
                .match_time = std::chrono::steady_clock::now(),
                .calculated_success_time_delay = DEFAULT_SUCCESS_DELAY_NS // Placeholder for actual calculation
            });
            pending_requests.erase(it);
            LOG_INFO("Entanglement request matched between node " + std::to_string(node_id) + " and neighbour " + std::to_string(neighbour_node) + ".");
            return true;
        }
    }
    //else we add a new pending request
    EntanglementRequest new_request;
    new_request.command = std::move(command);
    new_request.node_id = node_id;
    new_request.neighbour_node = neighbour_node;
    new_request.command_issue_time = std::chrono::steady_clock::now();

    // If a matching request is not found, add it to the pending requests
    pending_requests.push_back(std::move(new_request));
    LOG_INFO("Added new entanglement request from node " + std::to_string(node_id) + " to neighbour " + std::to_string(neighbour_node) + ".");
    return true;
}
