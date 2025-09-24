#include <memory>

#include "entanglement_manager.hpp"
#include "DataTypes/commands.hpp"

EntanglementManager::EntanglementManager(IProfiler *logger) : event_logger(logger) {}

void EntanglementManager::on_run()
{
    while (!abort)
    {
        // CommandData_t data;
        EntanglementRequest ent_request;
        // --- CRITICAL SECTION: only take an item; do nothing else while locked.
        {
            std::unique_lock<std::mutex> lock(this->data_lock);
            this->data_ready.wait_for(lock, std::chrono::microseconds(10000), [this]
                                      { return abort || !this->data_queue.empty(); }); // wait for 10 micro or until new data
            //if (event_logger != nullptr)
                // event_logger->log_event("Unlocked entanglement");
                //event_logger->pe_start("entanglement_event");
            if (!this->data_queue.empty())
            {
                // decide what to do with new data
                ent_request.entanglement_command = std::move(this->data_queue.front());
                this->data_queue.pop_front();

                // just verify not an empty command is sent
                if (ent_request.entanglement_command.response_callback == nullptr || ent_request.entanglement_command.command == nullptr)
                {
                    // check for notify on abort
                    if (abort && this->data_queue.empty())
                        break;
                    else
                        continue;
                }
                // form the entanglemnt request
                EntanglementAttempt *cmd = static_cast<EntanglementAttempt *>(ent_request.entanglement_command.command.get());
                ent_request.neighbour_node = cmd->nghbr;
                ent_request.command_issue_time = std::chrono::steady_clock::now();

                if (match_ent_pair(ent_request))
                {
                    // send the commands
                    // notify_runner(ent)
                    // notify_runner(ent_request)
                    // TODO: add notodication callback
                    break;
                }
                else
                {
                    entanglement_requests.push_back(std::move(ent_request));
                }
            }

        } // <-- unlock here before any heavy work or callbacks
        ResponseData dta;
        for (auto i = 0; i < entanglement_requests.size(); i++)
        {
            if (!check_pair_valid(entanglement_requests[i]))
            {
                // prepare the response data
                // TODO: prepare data
                // call response with ent sync event and discard
                entanglement_requests[i].entanglement_command.response_callback(dta);
                // remove from vector
                entanglement_requests.erase(entanglement_requests.begin() + i);
            }
        }
        //if (event_logger != nullptr)
            // event_logger->log_event("Unlocked entanglement");
            //event_logger->pe_stop("entanglement_event");
    }
}

bool EntanglementManager::check_pair_valid(const EntanglementRequest &request)
{
    std::chrono::duration elapsed_time = std::chrono::steady_clock::now() - request.command_issue_time;
    if (elapsed_time.count() > entanglement_pair_expiry_timeout)
    {
        return false;
    }
    return true;
}

bool EntanglementManager::match_ent_pair(const EntanglementRequest &request)
{
    for (auto &ent_entry : entanglement_requests)
    {
        if (request.neighbour_node == ent_entry.entanglement_command.node_id)
        {
            if (request.entanglement_command.node_id == ent_entry.neighbour_node)
            {
                return true;
            }
        }
    }
    return false;
}

void EntanglementManager::on_abort()
{
    // maybe run cleanup
    abort = true;
    this->notify(CommandData_t{});
}