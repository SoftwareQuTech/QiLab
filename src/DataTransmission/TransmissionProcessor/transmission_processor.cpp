#include "DataTransmission/transmission_processor.hpp"

#define COMMAND_MESSAGE_LENGTH      4
#define MAX_BUFFER_LENGTH           32
#define RESPONSE_MESSAGE_LENGTH     4
#define MAX_NUMBER_OF_COMMANDS     (MAX_BUFFER_LENGTH / COMMAND_MESSAGE_LENGTH)

// CRC-8/CCITT parameters.
static constexpr uint8_t CRC8_CCITT_POLY = 0x07u;
static constexpr uint8_t CRC8_INIT = 0x00u;
static constexpr uint8_t CRC8_XOROUT = 0x00u;

TransmissionProcessor::TransmissionProcessor(ITransmissionHandler *transmission_handler) : data_handler(transmission_handler) {}

TransmissionProcessor::~TransmissionProcessor() {}

int8_t TransmissionProcessor::receive_command(std::vector<std::unique_ptr<CommandBase>> &commands, uint8_t read_command_count)
{
    uint8_t data_buffer[MAX_BUFFER_LENGTH] = {0};

    if(read_command_count == 0 || read_command_count > MAX_NUMBER_OF_COMMANDS)
    {
        return -1;
    }

    if(data_handler->receive_data(data_buffer, COMMAND_MESSAGE_LENGTH * read_command_count))
    {
        return -2;
    }

    for(uint8_t i = 0; i < read_command_count; i++)
    {
        std::unique_ptr<CommandBase> cmd;
        cmd = deserialize_command(data_buffer);
        if(cmd != nullptr)
        {
            commands.push_back(std::move(cmd));
        }
        else
        {
            return -3;
        }
    }

    return 0;
}

int8_t TransmissionProcessor::send_response(std::vector<std::unique_ptr<ResponseData>> responses)
{
    uint8_t data_buffer[MAX_BUFFER_LENGTH] = {0};
    uint8_t response_data[RESPONSE_MESSAGE_LENGTH] = {0};
    if(responses.size() == 0 || responses.size() > MAX_NUMBER_OF_COMMANDS)
    {
        return -1;
    }

    for(auto i = 0; i < responses.size(); i++)
    {
        auto response = responses[i].get();
        if(response == nullptr)
        {
            return -2;
        }
        serialize_response(*response, response_data);
        std::copy(response_data, response_data + RESPONSE_MESSAGE_LENGTH, data_buffer + (i * RESPONSE_MESSAGE_LENGTH));
    }

    data_handler->send_data(data_buffer, RESPONSE_MESSAGE_LENGTH * responses.size());
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

bool TransmissionProcessor::verify_crc(const uint8_t *data, std::size_t len)
{
    if (len > 3)
        return false;

    if (data[3] != crc8_ccitt(data, len))
        return false;

    return true;
}

std::unique_ptr<CommandBase> TransmissionProcessor::deserialize_command(const uint8_t* received_data)
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

int8_t TransmissionProcessor::serialize_response(const ResponseData &response, uint8_t *buffer)
{
    buffer[0] = 0;//buffer[0] -> must be populated on hardware platform with actual cyclecount
    if (response.outcome_status & Outcome_Status_t::SUCCESS)
    {
        buffer[1] = (0xf0 & ((uint8_t)response.outcome_result << 4)) | (0x0f & (uint8_t)response.status);
    }
    else
    {
        buffer[1] = (0xf0 & ((uint8_t)response.outcome_status << 4)) | (0x0f & (uint8_t)response.status);
    }
    buffer[2] = 0;//buffer[2] -> must be populated on hardware platform with actual data with every cycle
    uint8_t crc = crc8_ccitt(buffer, 3);
    buffer[3] = crc; //this is quite useless, because it has to be recalculated on the hardware platform for SPI

    return 0;
}
