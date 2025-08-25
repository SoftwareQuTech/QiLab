/*
** Created on: 25.08.2025
** Created by: mrkolev
** Description: Contains command handler. This serializes and
**              deserializes the data from the communication
*/
#pragma once
#include <memory>
#include "CommandHandler/transmision_handler.hpp"
#include "DataTypes/commands.hpp"
#include "DataTypes/responses.hpp"

class CommandHandler
{
public:
    CommandHandler(ITransmissionHandler *);
    ~CommandHandler();

    /**
     * @brief Blocks until a command is received and decodes it.
     *
     * This call blocks the calling thread until a complete command frame is
     * available from the underlying transport. On success, the decoded command
     * is written into @p cmd.
     *
     * @param[out] cmd  Destination for the received and decoded command.
     *
     * @return Status code (implementation-defined).
     * @retval 0 Success (command stored in @p cmd).
     * @retval non-zero Error code indicating why no command was returned
     *                  (e.g., transport error, parse error, shutdown).
     *
     * @note This function may block indefinitely if no data arrives and no
     *       timeout mechanism is implemented in the transport layer.
     * @warning Not thread-safe unless the underlying transport is externally
     *          synchronized.
     */
    uint8_t receive_command(std::unique_ptr<CommandBase> &cmd);

    /**
     * @brief Sends a response over the underlying transport.
     *
     * Serializes and transmits the given response. The call returns a status
     * indicating whether the response was queued/sent successfully.
     *
     * @param[in] resp  Response payload to serialize and send.
     *
     * @return Status code (implementation-defined).
     * @retval 0 Success (response accepted/sent).
     * @retval non-zero Error code indicating failure
     *                  (e.g., transport not ready, write error).
     *
     * @warning Not thread-safe unless the underlying transport is externally
     *          synchronized.
     */
    uint8_t send_response(const ResponseData &resp);

private:
    /* A pointer to the desired data handler object*/
    ITransmissionHandler *data_handler;

    /*Desialize the command received from comuncation channel*/
    std::unique_ptr<CommandBase> deserialize_command(const uint8_t *);

    /*Serialize the response into an array that is ready to be sent*/
    uint8_t serialize_command(const ResponseData &, uint8_t *);

    CommandHandler(const CommandHandler &) = delete;
    CommandHandler(const CommandHandler &&) = delete;
    CommandHandler &operator=(CommandHandler &) = delete;
    CommandHandler &operator=(CommandHandler &&) = delete;
};