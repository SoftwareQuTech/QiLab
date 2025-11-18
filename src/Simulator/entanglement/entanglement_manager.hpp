#pragma once
#include <vector>
#include <chrono>
#include <memory>
#include <functional>

#include "DataTypes/commands.hpp"
#include "DataTypes/responses.hpp"

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

class EntanglementManager
{
public:
    EntanglementManager(std::function<void(int, Outcome_Status_t)>, std::function<void(int, std::unique_ptr<CommandBase>)>);
    //update the requests. 
    int update_requests();
    bool add_new_request(std::unique_ptr<CommandBase> command, uint16_t node_id, uint16_t neighbour_node);
protected:
    //run though pending requests and see if they are still valid
    bool check_request_is_expired(const EntanglementRequest &request);
    //try to match the request to another one
    bool match_request(const EntanglementRequest&);

private:

    static constexpr std::chrono::microseconds ENTANGLEMENT_TIMEOUT{500};
    static constexpr int DEFAULT_SUCCESS_DELAY_NS = 1000;

    std::function<void(int, Outcome_Status_t)> response_callback;
    std::function<void(int, std::unique_ptr<CommandBase>)> command_callback;
    std::vector<EntanglementRequest> pending_requests;
    std::vector<EntanglementPair> matched_pairs;
};
