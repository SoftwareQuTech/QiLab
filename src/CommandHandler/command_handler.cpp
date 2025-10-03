#include "command_handler.hpp"

#define COMMAND_MESSAGE_LENGTH 4
#define RESPONSE_MESSAGE_LENGTH 4
// CRC-8/CCITT parameters.
static constexpr uint8_t CRC8_CCITT_POLY = 0x07u;
static constexpr uint8_t CRC8_INIT = 0x00u;
static constexpr uint8_t CRC8_XOROUT = 0x00u;

CommandHandler::CommandHandler(ITransmissionHandler *transmission_handler) : data_handler(transmission_handler) {}
CommandHandler::CommandHandler(ITransmissionHandler *transmission_handler, IProfiler *logger) : data_handler(transmission_handler), local_logger(logger) {}

CommandHandler::~CommandHandler() {}

int8_t CommandHandler::receive_command(std::unique_ptr<CommandBase> &cmd)
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

int8_t CommandHandler::send_response(std::unique_ptr<ResponseData> response)
{
    uint8_t data_buffer[RESPONSE_MESSAGE_LENGTH];
    //response is simple, no need for extra overhead
    response->serialize_data(data_buffer);
    data_handler->send_data(data_buffer, RESPONSE_MESSAGE_LENGTH);
    return 0;
}

static uint8_t crc8_ccitt_update(uint8_t crc, uint8_t byte) noexcept
{
    crc ^= byte;
    for (uint8_t i = 0; i < 8; ++i)
    {
        crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ CRC8_CCITT_POLY)
                           : static_cast<uint8_t>(crc << 1);
    }
    return crc;
}
static uint8_t crc8_ccitt(const uint8_t *data, std::size_t len) noexcept
{
    uint8_t crc = CRC8_INIT;
    while (len--)
    {
        crc = crc8_ccitt_update(crc, *data++);
    }
    return static_cast<uint8_t>(crc ^ CRC8_XOROUT);
}

bool CommandHandler::verify_crc(const uint8_t *data, std::size_t len)
{
    if (len > 3)
        return false;

    if (data[3] != crc8_ccitt(data, len))
        return false;

    return true;
}

std::unique_ptr<CommandBase> CommandHandler::deserialize_command(const uint8_t* received_data)
{
    if(!verify_crc(received_data, 3))
    {
        return nullptr;
    }

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

int8_t CommandHandler::serialize_response(const ResponseData &response, uint8_t* buffer)
{
    //response.serialize_data(buffer);
    return -1;
}
