/*
** Created on: 23.09.2025
** Created by: mrkolev
** Description: Contains entanglement manager. This is a module
**              that is supposed to handle actual sync between nodes
**              and simulate the CR check.
*/

#pragma once
#include <cstdint>

#include <MessageBroker/message_broker.hpp>
#include "DataTypes/command_data.hpp"
#include "Simulator/entanglement/entanglement_manager.hpp"


class EntanglementRunner:
{
    public:
    EntanglementRunner(std::shared_ptr<MessageBroker>);
    ~EntanglementRunner() = default;

    int on_run();
    void on_abort();

    private:
    std::shared_ptr<MessageBroker> message_broker;
    EntanglementManager entanglement_manager;

    bool abort = false;
    
    EntanglementRunner(const EntanglementRunner&) = delete;
    EntanglementRunner(const EntanglementRunner&&) = delete;
    EntanglementRunner& operator=(EntanglementRunner&) = delete;
    EntanglementRunner& operator=(EntanglementRunner&&) = delete;
};