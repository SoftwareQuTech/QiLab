#pragma once
#include <atomic>
#include <cstdint>
#include <memory>

#include "DataTypes/commands.hpp"
#include "DataTypes/responses.hpp"
#include "MessageBroker/thread_safe_ring_buffer.hpp"

enum class DestinationType {
    SimRunner,
    CommandRunner,
    EntanglementRunner
};

struct MessageRoute {
    DestinationType source;
    DestinationType destination;
    
    bool is_valid_cmd_route() const noexcept {
        // Define valid routes
        if (source == DestinationType::CommandRunner) {
            return destination == DestinationType::SimRunner ||
                   destination == DestinationType::EntanglementRunner;
        }
        if (source == DestinationType::EntanglementRunner) {
            return destination == DestinationType::SimRunner;
        }
        return false;
    }
    bool is_valid_rsp_route() const noexcept
    {
        if (source == DestinationType::EntanglementRunner)
        {
            return destination == DestinationType::CommandRunner;
        }
        if (source == DestinationType::SimRunner)
        {
            return destination == DestinationType::CommandRunner;
        }
        return false;
    }
};

template <typename T>
struct TransportData : MessageRoute {
    std::unique_ptr<T> transport;
    uint16_t node_id;
};

class MessageBroker {
private:
    static constexpr size_t QUEUE_SIZE = 256;
    
    alignas(64) ThreadSafeRingBuffer<TransportData<CommandBase>, QUEUE_SIZE> sim_commands_;
    alignas(64) ThreadSafeRingBuffer<TransportData<CommandBase>, QUEUE_SIZE> ent_commands_;
    alignas(64) ThreadSafeRingBuffer<TransportData<ResponseData>, QUEUE_SIZE> cmd_responses_;

public:
    bool route_command(TransportData<CommandBase>&& cmd) noexcept;
    bool route_response(TransportData<ResponseData>&& resp) noexcept;
    /**
     * @brief Get a command for the specified runner type
     * 
     * @param cmd Output parameter that will receive the command
     * @param runner_type Type of runner requesting the command
     * @return true if a command was available and retrieved
     * @return false if no command was available
     */
    bool get_command(TransportData<CommandBase> &cmd, DestinationType runner_type) noexcept;
    bool get_response(TransportData<ResponseData>& resp) noexcept;
};


