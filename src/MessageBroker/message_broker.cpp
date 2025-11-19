#include "message_broker.hpp"
#include "logger/logger_api.hpp"
    
bool MessageBroker::route_command(TransportData<CommandBase> &&cmd) noexcept
{
    if (!cmd.is_valid_cmd_route())
    {
        LOG_ERROR("Invalid command route in MessageBroker.");
        return false;
    }

    switch (cmd.destination)
    {
    case DestinationType::SimRunner:
        return sim_commands_.push(std::move(cmd));
    case DestinationType::EntanglementRunner:
        return ent_commands_.push(std::move(cmd));
    default:
        LOG_ERROR("Unknown command destination in MessageBroker.");
        return false;
    }
}

bool MessageBroker::route_response(TransportData<ResponseData> &&resp) noexcept
{
    if (!resp.is_valid_rsp_route())
    {
        LOG_ERROR("Invalid response route in MessageBroker.");
        return false;
    }

    return cmd_responses_.push(std::move(resp));
}

bool MessageBroker::get_command(TransportData<CommandBase> &cmd, DestinationType runner_type) noexcept
{
    switch (runner_type)
    {
    case DestinationType::SimRunner:
        return sim_commands_.pop(cmd);
    case DestinationType::EntanglementRunner:
        return ent_commands_.pop(cmd);
    default:
        LOG_ERROR("Unknown command destination in MessageBroker.");
        return false;
    }
}

bool MessageBroker::get_response(TransportData<ResponseData> &resp) noexcept
{
    if (!cmd_responses_.pop(resp))
    {
        //LOG_WARNING("Failed to pop response from MessageBroker. Buffer is empty!");
        return false;
    }
    return true;
}