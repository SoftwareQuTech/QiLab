/*
** Created on: 23.09.2025
** Created by: mrkolev
** Description: Contains entanglement manager. This is a module
**              that is supposed to handle actual sync between nodes
**              and simulate the CR check.
*/

#pragma once
#include <cstdint>
#include <memory>

#include <MessageBroker/message_broker.hpp>
#include "DataTypes/command_data.hpp"
#include "Simulator/entanglement/entanglement_manager.hpp"


class EntanglementRunner
{
    public:
    EntanglementRunner(std::shared_ptr<MessageBroker>);
    ~EntanglementRunner() = default;

    int on_run(int core_id = -1);
    void on_abort();

    protected:
    //callback for sending response back to the node runner from manager
    void send_response_async(int node_id, Outcome_Status_t status);
    //callback for sending command back to the node runner from manager
    void send_command_async(int node_id, std::unique_ptr<CommandBase> cmd);

    private:
    std::shared_ptr<MessageBroker> message_broker;
    std::unique_ptr<EntanglementManager> entanglement_manager;

    bool abort = false;
    
    EntanglementRunner(const EntanglementRunner&) = delete;
    EntanglementRunner(const EntanglementRunner&&) = delete;
    EntanglementRunner& operator=(EntanglementRunner&) = delete;
    EntanglementRunner& operator=(EntanglementRunner&&) = delete;
};