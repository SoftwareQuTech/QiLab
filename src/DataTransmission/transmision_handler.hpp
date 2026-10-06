// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

/*
** Created on: 25.08.2025
** Created by: mrkolev
** Description: Contains the ITransmissionHandler interface for device communication
*/

#pragma once
#include <cstdint>

/**
 * @brief Abstract interface for transmission handlers.
 *
 * This interface defines the contract for sending, receiving, and managing raw
 * byte data transmission between a device and external mediums (e.g., UART, SPI, 
 * network, or other communication protocols).
 *
 * Implementations of this interface must support:
 * - Unidirectional data transmission (send/receive operations)
 * - Bidirectional simultaneous data transfer (SPI-style operations)
 * - Buffer management and flushing
 *
 * @note All implementations must be thread-safe if used in multithreaded contexts.
 * @note Return values use int8_t for status indication; specific status codes should
 *       be defined by concrete implementations.
 */
struct ITransmissionHandler
{
    /// Virtual destructor to allow safe polymorphic cleanup.
    virtual ~ITransmissionHandler() = default; 

    /**
     * @brief Receives data from the communication medium into the provided buffer.
     *
     * This method reads incoming data and stores it in the provided buffer.
     * It is implementation-dependent whether this operation is blocking or non-blocking.
     *
     * @param[out] buffer Pointer to the destination buffer where received data will be stored.
     *                    Must not be nullptr.
     * @param[in]  length Maximum number of bytes to receive into the buffer.
     * 
     * @return Status code indicating success or error:
     *         - Positive values: number of bytes successfully received
     *         - Zero or negative: implementation-specific error codes
     */
    virtual int8_t receive_data(uint8_t* buffer, uint8_t length) = 0;

    /**
     * @brief Sends data from the provided buffer to the communication medium.
     *
     * This method transmits data from the source buffer. It is implementation-dependent
     * whether this operation is blocking or non-blocking.
     *
     * @param[in] buffer Pointer to the source buffer containing data to send.
     *                   Must not be nullptr.
     * @param[in] length Number of bytes to send from the buffer.
     * 
     * @return Status code indicating success or error:
     *         - Positive values: number of bytes successfully sent
     *         - Zero or negative: implementation-specific error codes
     */
    virtual int8_t send_data(const uint8_t* buffer, uint8_t length) = 0;
    
    /**
     * @brief Flushes any pending data in internal buffers.
     *
     * This method ensures all buffered data is transmitted or received as applicable.
     * Useful for ensuring data integrity before switching communication modes or 
     * shutting down the handler.
     */
    virtual void flush() = 0;

    /**
     * @brief Performs simultaneous bidirectional data transfer.
     *
     * This method is typically used for SPI-style communication where data is
     * transmitted and received simultaneously. The tx_buffer contains data to send,
     * and received data is stored in rx_buffer.
     *
     * @param[in]  tx_buffer Pointer to the transmission buffer (data to send).
     *                       Must not be nullptr.
     * @param[out] rx_buffer Pointer to the reception buffer (data received).
     *                       Must not be nullptr.
     * @param[in]  length    Number of bytes to transfer in both directions.
     * 
     * @return Status code indicating success or error:
     *         - Positive values: number of bytes successfully transferred
     *         - Zero or negative: implementation-specific error codes
     */
    virtual int8_t transfer(uint8_t const* tx_buffer, uint8_t const* rx_buffer, const uint8_t length) = 0;
};