#include "command_handler.hpp"

#define COMMAND_MESSAGE_LENGTH          4
#define RESPONSE_MESSAGE_LENGTH         4

CommandHandler::CommandHandler(ITransmissionHandler* transmission_handler) : data_handler(transmission_handler) {}
CommandHandler::CommandHandler(ITransmissionHandler* transmission_handler, IProfiler* logger) : data_handler(transmission_handler), local_logger(logger) {}

CommandHandler::~CommandHandler() {}

uint8_t CommandHandler::receive_command(std::unique_ptr<CommandBase> &cmd)
{
    uint8_t data_buffer[COMMAND_MESSAGE_LENGTH] = {0};

    if(data_handler->receive_data(data_buffer, COMMAND_MESSAGE_LENGTH))
    {
        return -1;
    }

    cmd = deserialize_command(data_buffer);

    if(cmd == nullptr)
    {
        return -2;
    }

    return 0;
}

uint8_t CommandHandler::send_response(const ResponseData &response)
{
    uint8_t data_buffer[RESPONSE_MESSAGE_LENGTH];
    //response is simple, no need for extra overhead
    response.serialize_data(data_buffer);
    data_handler->send_data(data_buffer, RESPONSE_MESSAGE_LENGTH);
    return 0;
}


std::unique_ptr<CommandBase> CommandHandler::deserialize_command(const uint8_t* received_data)
{
    switch (received_data[0]) {
        case Command_t::QUBIT_INIT:                             return std::make_unique<QubitInitialize>(received_data);
        case Command_t::ENTANGLEMENT_SWAP:                      return std::make_unique<EntanglementSwap>(received_data);
        case Command_t::NOP:                                    return std::make_unique<NOP_Command>(received_data);
        case Command_t::SINGLE_QUBIT_GATE:                      return std::make_unique<SingleQubitGate>(received_data);
        case Command_t::TWO_QUBIT_GATE:                         return std::make_unique<TwoQubitGate>(received_data);
        case Command_t::QUBIT_MEASUREMENT:                      return std::make_unique<QubitMeasure>(received_data);
        case Command_t::ENTANGLEMENT_ATTEMPT:                   return std::make_unique<EntanglementAttempt>(received_data);
        case Command_t::ENTANGLEMENT_MEASURE:                   return std::make_unique<EntangleMeasure>(received_data);
        case Command_t::PRE_MEASUREMENT_GATES_SELECTION:        return std::make_unique<PreMeasureGatesSelection>(received_data);
        case Command_t::QUBIT_MOVE:                             return std::make_unique<QubitMove>(received_data);
        case Command_t::QUBIT_SWAP:                             return std::make_unique<QubitSwap>(received_data);
        default: return nullptr; //safer that static casting unknown
    }
}

uint8_t CommandHandler::serialize_response(const ResponseData &response, uint8_t* buffer)
{
    //response.serialize_data(buffer);
    return -1;
}
