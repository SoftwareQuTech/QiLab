#pragma once
#include <vector>
#include <chrono>
#include <memory>
#include <functional>

#include "DataTypes/commands.hpp"
#include "DataTypes/responses.hpp"

class EntanglementRequest;
class EntanglementPair;

class EntanglementManager
{
public:
    EntanglementManager(std::function<void(int, Outcome_Status_t)>, std::function<void(int, std::unique_ptr<CommandBase>)>);
    //update the requests. If any entanglement commands are ready will be addded to the vector and the number will be returned
    int update_requests(std::vector<std::unique_ptr<CommandBase>>);
    bool add_new_request(std::unique_ptr<CommandBase>);
protected:
    //track time on the matched pairs after calculating time to success
    //if a matched pair is ready to be sent, the index fo the vector item is returned
    int check_pair_ready();
    //run though pending requests and see if they are still valid
    bool check_request_is_expired(const EntanglementRequest &request);
    //try to match the request to another one
    bool match_request(const EntanglementRequest&);

private:
    std::function<void(int, Outcome_Status_t)> response_callback;
    std::function<void(int, std::unique_ptr<CommandBase>)> command_callback;
    std::vector<EntanglementRequest> pending_requests;
    std::vector<EntanglementPair> matched_pairs;
};
