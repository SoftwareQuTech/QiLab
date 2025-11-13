#include "message_broker.hpp"

bool MessageBroker::route_command(TransportData<CommandBase> &&cmd) noexcept
{
    if (!cmd.is_valid_route())
    {
        return false;
    }

    switch (cmd.destination)
    {
    case DestinationType::SimRunner:
        return sim_commands_.push(std::move(cmd));
    case DestinationType::EntanglementRunner:
        return ent_commands_.push(std::move(cmd));
    default:
        return false;
    }
}

bool MessageBroker::route_response(TransportData<ResponseData> &&resp) noexcept
{
    if (!resp.is_valid_route())
    {
        return false;
    }

    if (resp.source != DestinationType::SimRunner ||
        resp.destination != DestinationType::CommandRunner)
    {
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
        return false;
    }
}

bool MessageBroker::get_response(TransportData<ResponseData> &resp) noexcept
{
    return cmd_responses_.pop(resp);
}