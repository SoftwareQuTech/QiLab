// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

/*
** Created on: 25.08.2025
** Created by: mrkolev
** Description: Contains Transmission Processor module. This serializes and
**              deserializes the data from the communication into vectors
*/

#pragma once
#include <memory>
#include <vector>
#include <array>
#include <cstdint>

#include "DataTransmission/transmision_handler.hpp"
#include "DataTypes/commands.hpp"
#include "DataTypes/responses.hpp"
#include "logger/profiler_api.hpp"

#define COMMAND_MESSAGE_LENGTH      4
#define MAX_BUFFER_LENGTH          64
#define RESPONSE_MESSAGE_LENGTH     4
#define CONTROL_WORD_LEN           4
#define MAX_NUMBER_OF_COMMANDS     ((MAX_BUFFER_LENGTH - CONTROL_WORD_LEN) / COMMAND_MESSAGE_LENGTH)
#define LP_DATA_BUFFER_LENGTH      32   
/**
 * @brief Per-node transmission state data.
 *
 * Holds cycle counter and LP data buffer for a single node.
 * This allows each node to maintain independent transmission state.
 */
struct NodeTransmissionData
{
    uint64_t cycle_count = 0;
    std::array<uint32_t, LP_DATA_BUFFER_LENGTH> lp_data_buffer;
    uint64_t crc_error_count = 0;
    NodeTransmissionData() : lp_data_buffer{} {}
};

/**
 * @brief Handles reception and transmission of single commands/responses,
 *        plus synchronous request/response exchanges.
 *
 * The TransmissionProcessor bridges an ITransmissionHandler (transport) with
 * command/response objects. `receive_command()` and `send_response()` operate
 * on a **single** command/response and are **blocking** calls. For synchronous
 * channels (e.g., SPI full-duplex) use `transmit_msg()` to **transmit and
 * receive in one operation**.
 *
 * @note Copy/move disabled to avoid duplicating the transport binding.
 */
class TransmissionProcessor
{
private:
    // Static buffers to avoid stack allocation overhead
    // Aligned buffers for DMA efficiency
    __attribute__((aligned(64))) static uint8_t tx_buffer[MAX_BUFFER_LENGTH];
    __attribute__((aligned(64))) static uint8_t rx_buffer[MAX_BUFFER_LENGTH];
public:
    /**
     * @brief Bind the processor to a transport implementation.
     * @param[in] handler Unique pointer to a valid ITransmissionHandler.
     *                    The processor maintains a unique ownership of @p handler.
     */
    TransmissionProcessor(std::unique_ptr<ITransmissionHandler> handler);

    /// @brief Trivial destructor.
    ~TransmissionProcessor();

    /**
     * @brief Blocking receive of a single command frame.
     *
     * Waits for one complete command, verifies CRC, decodes it, and returns it
     * via @p command.
     *
     * @param[out] command Decoded command (reset and replaced on success).
     * @param[in]  timeout Timeout in milliseconds; 0 may block indefinitely
     *                     (transport-dependent).
     * @return int8_t
     *         - 0  on success
     *         - -1 invalid argument(s)
     *         - -2 transport timeout / read error
     *         - -3 CRC or opcode decode failure
     */
    int8_t receive_command(std::unique_ptr<CommandBase> &command, uint16_t timeout);

    /**
     * @brief Blocking send of a single response frame.
     *
     * Serializes one response, computes CRC, and transmits it via the transport.
     *
     * @param[in] response Response to send (must not be nullptr).
     * @return int8_t
     *         - 0  on success
     *         - -1 invalid argument(s)
     *         - -2 transport write error
     */
    int8_t send_response(const std::unique_ptr<ResponseData> &response);

    /**
     * @brief Synchronous transmit/receive for full-duplex channels (e.g., SPI).
     *
     * Serializes up to @p transfer_msg_count responses into a pre-allocated aligned buffer,
     * transmits them via SPI, and simultaneously receives the same number of command frames.
     * Received commands are decoded and swapped into @p commands at their respective indices.
     * This call is **blocking** and **atomic** at the SPI protocol level.
     *
     * Performance: Uses pre-allocated static buffers (tx_buffer, rx_buffer) aligned for
     * DMA efficiency. Commands are deserialized with minimal allocation overhead.
     *
     * @param[in]  responses           Responses to serialize and send (size must match transfer_msg_count).
    * @param[out] commands            Command vector; successfully decoded commands swap into indices [0..transfer_msg_count-1].
    *                               If CRC or deserialize fails for a slot, that entry is reset to nullptr.
     * @param[in]  transfer_msg_count  Number of messages to TX/RX atomically (≤ responses.size(), ≤ commands.size()).
     * @param[in,out] node_data_vec    Vector of per-node transmission data; processor extracts cycle counter and LP data per node during batch transmission.
     * @return int8_t
     *         - 0  on full success (TX/RX complete; all commands decoded)
     *         - -1 invalid sizes/counts or null entries
     *         - -2 transport I/O error
     *         - -3 CRC or opcode decode failure
     */
    int8_t transmit_msg(const std::vector<std::unique_ptr<ResponseData>> &responses,
                        std::vector<std::unique_ptr<CommandBase>> &commands,
                        uint8_t transfer_msg_count,
                        std::vector<NodeTransmissionData> &node_data_vec);

private:
    /// Shared pointer to the transport handler used for sending/receiving raw data.
    std::unique_ptr<ITransmissionHandler> data_handler;

    /**
     * @brief Decode a 4-byte command frame into a concrete CommandBase.
     *
     * Validates CRC over the first 3 bytes (byte 4 holds CRC), then dispatches by opcode.
     *
     * @param[in] buffer Pointer to the first byte of the 4-byte command frame.
     * @return std::unique_ptr<CommandBase> Concrete command on success; nullptr on CRC/opcode error.
     */
    std::unique_ptr<CommandBase> deserialize_command(const uint8_t *buffer);

    /**
     * @brief Verify CRC-8/CCITT of a 4-byte frame.
     *
     * Computes CRC for @p data[0..len-1] (expect len==3) and compares against @p data[3].
     *
     * @param[in] data Pointer to frame start.
     * @param[in] len  Number of covered bytes (expects 3).
     * @return true if CRC matches; false otherwise.
     */
    bool verify_crc(const uint8_t* data, std::size_t len);

    /**
     * @brief Serialize a response into a 4-byte frame.
     *
     * Layout:
     * - Byte 0: platform cycle counter (set to 0; platform should populate at runtime)
     * - Byte 1: hi-nibble = (outcome_status==SUCCESS ? outcome_result : outcome_status); lo-nibble = status
     * - Byte 2: platform-defined runtime byte (set to 0; platform should populate at runtime)
     * - Byte 3: CRC-8/CCITT of bytes [0..2]
     *
     * @param[in]  resp   Response object to serialize.
     * @param[out] buffer Destination buffer (size ≥ 4).
     * @return int8_t 0 on success; -1 on invalid input.
     */
    int8_t serialize_response(const ResponseData &resp, uint8_t *buffer);

    // Disable copy/move
    TransmissionProcessor(const TransmissionProcessor &) = delete;
    TransmissionProcessor(const TransmissionProcessor &&) = delete;
    TransmissionProcessor &operator=(TransmissionProcessor &) = delete;
    TransmissionProcessor &operator=(TransmissionProcessor &&) = delete;
};
