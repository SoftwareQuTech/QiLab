#include "Simulator/entanglement/entanglement_manager.hpp"

struct EntanglementRequest
{
    int neighbour_node;
    int node_id;
    std::unique_ptr<CommandBase> command;
    std::chrono::steady_clock::time_point command_issue_time;
};

struct EntanglementPair
{
    std::unique_ptr<CommandBase> command1;
    std::unique_ptr<CommandBase> command2;
    int node_id_1;
    int node_id_2;
    std::chrono::steady_clock::time_point match_time;
    int calculated_success_time_delay;
};

EntanglementManager::EntanglementManager(std::function<void(int, Outcome_Status_t)> resp_cb, std::function<void(int, std::unique_ptr<CommandBase>)> cmd_cb) : response_callback(resp_cb), command_callback(cmd_cb)
{
}

bool EntanglementManager::check_request_is_expired(const EntanglementRequest &request)
{
    const std::chrono::microseconds entanglement_pair_expiry_timeout(500); //[us]

    std::chrono::duration elapsed_time = std::chrono::steady_clock::now() - request.command_issue_time;
    if (std::chrono::microseconds(elapsed_time.count()) > entanglement_pair_expiry_timeout)
    {
        return false;
    }

    return true;
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


int EntanglementManager::check_pair_ready()
{
    //run through matched pairs and see if the success time expired
    int ret_indx = -1; //no index found



    return ret_indx;
}


int EntanglementManager::update_requests(std::vector<std::unique_ptr<CommandBase>>)
{
    // check pending requests
    if(pending_requests.size() > 0)
    {
        for(auto i = pending_requests.begin(); i != pending_requests.end(); i++)
        {
            if(check_request_is_expired(*i))
            {
                //this returns data to the runner and adds the reponse to the message broker
                response_callback(i->node_id, Outcome_Status_t::ENTANGLEMENT_SYNC_FAILURE);
                pending_requests.erase(i);
                break;
            }
        }
    }

    if (matched_pairs.size() > 0)
    {
        for (int i = 0; i < matched_pairs.size(); i++)
        {
            std::chrono::duration elapsed_time = std::chrono::steady_clock::now() - matched_pairs[i].match_time;
            if (elapsed_time > std::chrono::nanoseconds(matched_pairs[i].calculated_success_time_delay))
            {
                command_callback(matched_pairs[i].node_id_1, std::move(matched_pairs[i].command1));
                command_callback(matched_pairs[i].node_id_2, std::move(matched_pairs[i].command2));
                break;
            }
        }
    }
    return false;
}

bool EntanglementManager::add_new_request(std::unique_ptr<CommandBase> command)
{
    return false;
}
