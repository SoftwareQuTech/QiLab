/*
** Created on: 25.08.2025
** Created by: mrkolev
** Description: Contains the ITransmission interface
*/

#pragma once
#include <cstdint>

/**
 * @brief Interface for transmission handlers (abstract base class).
 *
 * This interface defines the contract for sending and receiving raw
 * byte data between a device and an external medium (e.g. UART, SPI, network).
 *
 * Classes implementing this interface must provide definitions for
 * both send and receive operations.
 */
struct ITransmissionHandler
{
    /// Virtual destructor to allow safe polymorphic cleanup.
    virtual ~ITransmissionHandler() = default; 

    /**
     * @brief Receives data into the provided buffer.
     *
     * @param[out] buffer Pointer to the destination buffer.
     * @param[in]  length Maximum number of bytes to receive.
     * @return Status of the operation TBD
     */
    virtual int8_t receive_data(uint8_t* buffer, uint8_t length) = 0;

    /**
     * @brief Sends data from the provided buffer.
     *
     * @param[in] buffer Pointer to the source buffer.
     * @param[in] length Number of bytes to send.
     * @return Status of the operation TBD
     */
    virtual int8_t send_data(const uint8_t* buffer, uint8_t length) = 0;
};