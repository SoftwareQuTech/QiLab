/*
** Created on: 25.08.2025
** Created by: mrkolev
** Description: Contains command handler. This serializes and
**              deserializes the data from the communication
*/

#pragma once
#include <memory>
#include <vector>
#include <cstdint>

#include "DataTransmission/transmision_handler.hpp"
#include "DataTypes/commands.hpp"
#include "DataTypes/responses.hpp"
#include "logger/profiler_api.hpp"

/**
 * @brief Handles the reception and transmission of commands and responses.
 *
 * The CommandHandler acts as the interface between the transport layer
 * (provided by an ITransmissionHandler) and the higher-level command/
 * response abstractions. It serializes outgoing responses into raw frames,
 * and deserializes incoming frames into Command objects.
 *
 * Typical usage:
 * - Construct with a pointer to a concrete ITransmissionHandler implementation.
 * - Call receive_command() to block until a command arrives and is decoded.
 * - Call send_response() to serialize and transmit a response.
 *
 * @note Copy and move operations are disabled to avoid multiple handlers
 *       owning the same transport.
 */
class TransmissionProcessor
{
public:
    /**
     * @brief Construct a new CommandHandler.
     *
     * @param[in] handler Pointer to a valid transport implementation.
     *                    The CommandHandler does not take ownership; the
     *                    caller must ensure the lifetime exceeds this object.
     */
    TransmissionProcessor(ITransmissionHandler *handler);

    /// Destructor. Does not delete the underlying transport handler.
    ~TransmissionProcessor();

    /**
     * @brief Blocks until a command is received and decodes it.
     *
     * This call blocks the calling thread until a complete commands frames are
     * available from the underlying transport. On success, the decoded commands
     * are written into @p commands.
     *
     * @param[out] commands  Destination for the received and decoded commands.
     *                          They can be multiple depending on how much nodes were registered
     *
     * @param[in] read_command_count The number of commands to receive and decode.
     *                                  Default is 1.
     * @return Status code (implementation-defined).
     * @retval 0 Success (commands stored in @p commands).
     * @retval non-zero Error code indicating why no command was returned
     *                  (e.g., transport error, parse error, shutdown).
     *
     * @note This function may block indefinitely if no data arrives and no
     *       timeout mechanism is implemented in the transport layer.
     * @warning Not thread-safe unless the underlying transport is externally
     *          synchronized.
     */
    int8_t receive_command(std::vector<std::unique_ptr<CommandBase>> &commands, uint8_t read_command_count = 1);

    /**
     * @brief Sends a response over the underlying transport.
     *
     * Serializes and transmits the given response. The call returns a status
     * indicating whether the response was queued/sent successfully.
     *
     * @param[in] responses  Response payload to serialize and send.
     *
     * @return Status code (implementation-defined).
     * @retval 0 Success (response accepted/sent).
     * @retval non-zero Error code indicating failure
     *                  (e.g., transport not ready, write error).
     *
     * @warning Not thread-safe unless the underlying transport is externally
     *          synchronized.
     */
    int8_t send_response(std::vector<std::unique_ptr<ResponseData>> responses);

private:
    /// Pointer to the transport handler used for sending/receiving raw data.
    ITransmissionHandler *data_handler;

    /**
     * @brief Deserializes a raw byte buffer into a Command object.
     *
     * @param[in] buffer Raw bytes received from the transport.
     * @return A unique_ptr to a decoded CommandBase instance.
     * @retval nullptr if the buffer does not represent a valid command.
     */
    std::unique_ptr<CommandBase> deserialize_command(const uint8_t *buffer);

    bool verify_crc(const uint8_t* data, std::size_t len);

    /**
     * @brief Serializes a response into a raw byte array.
     *
     * @param[in]  resp   Response object to serialize.
     * @param[out] buffer Destination buffer for serialized data.
     *
     * @return status of the operation
     */
    int8_t serialize_response(const ResponseData &resp, uint8_t *buffer);

    // Disable copy/move
    TransmissionProcessor(const TransmissionProcessor &) = delete;
    TransmissionProcessor(const TransmissionProcessor &&) = delete;
    TransmissionProcessor &operator=(TransmissionProcessor &) = delete;
    TransmissionProcessor &operator=(TransmissionProcessor &&) = delete;
};
