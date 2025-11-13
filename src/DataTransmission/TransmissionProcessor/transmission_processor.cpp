#include "DataTransmission/TransmissionProcessor/transmission_processor.hpp"
#include <iostream>
#define COMMAND_MESSAGE_LENGTH      4


#define MAX_BUFFER_LENGTH           64
#define RESPONSE_MESSAGE_LENGTH     4
#define CONTROL_WORD_LEN            4
#define MAX_NUMBER_OF_COMMANDS     ((MAX_BUFFER_LENGTH - CONTROL_WORD_LEN) / COMMAND_MESSAGE_LENGTH)



#define ASSERT_INPUT_RANGE(x, min, max) \
    if ((x) <= (min) || (x) > (max)) { \
        return -1; \
    } \


    // Define static buffers
__attribute__((aligned(64))) uint8_t TransmissionProcessor::tx_buffer[MAX_BUFFER_LENGTH];
__attribute__((aligned(64))) uint8_t TransmissionProcessor::rx_buffer[MAX_BUFFER_LENGTH];

// CRC-8/CCITT parameters.
static constexpr uint8_t CRC8_CCITT_POLY = 0x07u;
static constexpr uint8_t CRC8_INIT = 0x00u;
static constexpr uint8_t CRC8_XOROUT = 0x00u;

TransmissionProcessor::TransmissionProcessor(std::unique_ptr<ITransmissionHandler> transmission_handler) : data_handler(std::move(transmission_handler)) {}

TransmissionProcessor::~TransmissionProcessor() {}

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

int8_t TransmissionProcessor::transmit_msg(const std::vector<std::unique_ptr<ResponseData>> &responses, std::vector<std::unique_ptr<CommandBase>> &commands, uint8_t transfer_msg_count)
{
    // Using class-level buffers, no need to reallocate or zero
    static_assert(sizeof(TransmissionProcessor::tx_buffer) % 64 == 0, "tx_buffer not aligned");
    static_assert(sizeof(TransmissionProcessor::rx_buffer) % 64 == 0, "rx_buffer not aligned");

    ASSERT_INPUT_RANGE(responses.size(), 0, MAX_NUMBER_OF_COMMANDS);
    ASSERT_INPUT_RANGE(commands.size(), 0, MAX_NUMBER_OF_COMMANDS);
    ASSERT_INPUT_RANGE(transfer_msg_count, 0, responses.size());
    ASSERT_INPUT_RANGE(transfer_msg_count, 0, commands.size());

    if(responses.size() != commands.size())
    {
        return -1;
    }
    
    // Directly serialize into tx_buffer to avoid double copy
    for(auto i = 0; i < transfer_msg_count; i++)
    {
        auto response = responses[i].get();
        if(response == nullptr)
        {
            return -2;
        }
        uint8_t* dest = TransmissionProcessor::tx_buffer + (i * RESPONSE_MESSAGE_LENGTH);
        dest[0] = 0; // cycle count placeholder
        if (response->outcome_status == Outcome_Status_t::SUCCESS)
        {
            dest[1] = (0xf0 & ((uint8_t)response->outcome_result << 4)) | (0x0f & (uint8_t)response->status);
        }
        else
        {
            dest[1] = (0xf0 & ((uint8_t)response->outcome_status << 4)) | (0x0f & (uint8_t)response->status);
        }
        dest[2] = 0; // data placeholder
        dest[3] = crc8_ccitt(dest, 3); // CRC for this message
    }

    if(data_handler->transfer(tx_buffer, rx_buffer, RESPONSE_MESSAGE_LENGTH * transfer_msg_count))
    {
        std::cerr << "Failed to transfer data." << std::endl;
        return -3;
    }

    // Process commands directly from rx_buffer
    for(uint8_t i = 0; i < transfer_msg_count; i++)
    {
        const uint8_t* cmd_data = rx_buffer + (i * COMMAND_MESSAGE_LENGTH);
        if(!verify_crc(cmd_data, 3))
        {
            std::cerr << "CRC verification failed." << std::endl;
            return -4;
        }
        auto cmd = deserialize_command(cmd_data);
        if(cmd != nullptr)
        {
            commands[i].swap(cmd);
        }
        else
        {
            std::cerr << "Command is not recognized." << std::endl;
            return -4;
        }
    }

    return 0;
}

int8_t TransmissionProcessor::receive_command(std::unique_ptr<CommandBase> &command, uint16_t timeout)
{
    uint8_t data_buffer[COMMAND_MESSAGE_LENGTH] = {0};

    if (data_handler->receive_data(data_buffer, COMMAND_MESSAGE_LENGTH))
    {
        return -2;
    }
    std::unique_ptr<CommandBase> cmd = deserialize_command(data_buffer);
    
    if (cmd != nullptr)
    {
        command = std::move(cmd);
    }
    else
    {
        return -3;
    }

    return 0;
}

int8_t TransmissionProcessor::send_response(const std::unique_ptr<ResponseData> &response) 
{
    uint8_t response_data[RESPONSE_MESSAGE_LENGTH] = {0};

    if (response == nullptr)
    {
        return -1;
    }
    serialize_response(*response, response_data);

    if (data_handler->send_data(response_data, RESPONSE_MESSAGE_LENGTH))
    {
        return -2;
    }

    return 0;
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
    // Fast bounds check first
    const uint8_t cmd_type = received_data[0];
    if (cmd_type > Command_t::QUBIT_SWAP) {
        return nullptr;
    }
    
    // Hot path optimization: Check most common commands first
    switch (cmd_type) {
        case Command_t::SINGLE_QUBIT_GATE:
            return std::make_unique<SingleQubitGate>(received_data);
        case Command_t::TWO_QUBIT_GATE:
            return std::make_unique<TwoQubitGate>(received_data);
        case Command_t::QUBIT_MEASUREMENT:
            return std::make_unique<QubitMeasure>(received_data);
        case Command_t::NOP:
            return std::make_unique<NOP_Command>(received_data);
            
        // Less common operations
        case Command_t::QUBIT_INIT:
            return std::make_unique<QubitInitialize>(received_data);
        case Command_t::ENTANGLEMENT_SWAP:
            return std::make_unique<EntanglementSwap>(received_data);
        case Command_t::ENTANGLEMENT_ATTEMPT:
            return std::make_unique<EntanglementAttempt>(received_data);
        case Command_t::ENTANGLEMENT_MEASURE:
            return std::make_unique<EntangleMeasure>(received_data);
        case Command_t::PRE_MEASUREMENT_GATES_SELECTION:
            return std::make_unique<PreMeasureGatesSelection>(received_data);
        case Command_t::QUBIT_MOVE:
            return std::make_unique<QubitMove>(received_data);
        case Command_t::QUBIT_SWAP:
            return std::make_unique<QubitSwap>(received_data);
        default:
            return nullptr;
    }
}

int8_t TransmissionProcessor::serialize_response(const ResponseData &response, uint8_t *buffer)
{
    buffer[0] = 0;//buffer[0] -> must be populated on hardware platform with actual cyclecount
    if (response.outcome_status == Outcome_Status_t::SUCCESS)
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
