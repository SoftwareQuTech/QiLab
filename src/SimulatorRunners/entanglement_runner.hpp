/*
** Created on: 23.09.2025
** Created by: mrkolev
** Description: Contains entanglement manager. This is a module
**              that is supposed to handle actual sync between nodes
**              and simulate the CR check.
*/

#pragma once

#include "runnable.hpp"
#include "DataTypes/command_data.hpp"
#include "logger/logger.hpp"

struct EntanglementRequest
{
    int neighbour_node;
    CommandData_t entanglement_command;
    std::chrono::steady_clock::time_point command_issue_time;
};

class EntanglementManager: public IRunnable<CommandData_t>
{
    public:
    EntanglementManager(IProfiler*);
    ~EntanglementManager() = default;

    int on_run() override;
    void on_abort() override;

    private:
    //checks if this pair is still valid based on timestamp
    inline bool check_pair_valid(const EntanglementRequest &request);
    //try to match a pair for entanglement, and if it does, issue the commands to the runner
    bool match_ent_pair(const EntanglementRequest &request);

    std::vector<EntanglementRequest> entanglement_requests;
    IProfiler* event_logger;

    bool abort = false;
    const int entanglement_pair_expiry_timeout = 500000; //[ns]
    EntanglementManager(const EntanglementManager&) = delete;
    EntanglementManager(const EntanglementManager&&) = delete;
    EntanglementManager& operator=(EntanglementManager&) = delete;
    EntanglementManager& operator=(EntanglementManager&&) = delete;
};