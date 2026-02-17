/*
** Created on: 16.10.2025
** Created by: mrkolev
** Description: SPI master handler for full-duplex communication with quantum devices.
**              Provides synchronized transmit/receive operations over SPI interface.
*/

#pragma once
#include <memory>
#include <vector>
#include <cstdint>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#include "DataTransmission/transmision_handler.hpp"

/**
 * @class SPIHandler
 * @brief SPI master communication handler implementing full-duplex transfers.
 * 
 * This class provides SPI master functionality for communicating with quantum node
 * devices via the Linux spidev interface. It supports configurable SPI parameters
 * and synchronized transmit/receive operations using a pre-allocated transfer template
 * for optimal performance.
 * 
 * @note Full-duplex transfers are atomic; data is transmitted and received simultaneously.
 * @note Requires /dev/spidevX.Y device with appropriate permissions.
 */
class SPIHandler: public ITransmissionHandler
{
public:
    /**
     * @brief Construct a new SPI handler instance.
     * Initializes with default SPI parameters (mode 0, 8-bit words, 15MHz clock).
     */
    SPIHandler();
    
    /**
     * @brief Destroy the SPI handler and close the SPI device file descriptor.
     */
    ~SPIHandler() override;
    
    /**
     * @brief Connect to and configure an SPI device.
     * 
     * @param device_path Path to the spidev device (default: "/dev/spidev1.0")
     * @param spi_mode    SPI mode (0-3, default: 0 for CPOL=0, CPHA=0)
     * @param bits_per_word Bits per word (default: 8)
     * @param max_speed_hz  Clock frequency in Hz (default: 20MHz)
     * 
     * @return 0 on success, negative error code on failure
     * @retval -1 Device open/ioctl failed
     * @retval -2 Invalid parameters
     */
    int8_t connect_spi(const char* device_path = "/dev/spidev1.0", 
                      uint8_t spi_mode = 0, 
                      uint8_t bits_per_word = 8, 
                      uint32_t max_speed_hz = 20000000);
    
    /**
     * @brief Receive data from SPI device (half-duplex read).
     * 
     * @param buffer Destination buffer for received data
     * @param length Number of bytes to receive
     * 
     * @return 0 on success, negative error code on failure
     * @retval -1 SPI device not connected
     * @retval -2 ioctl failed
     */
    int8_t receive_data(uint8_t* buffer, uint8_t length) override;
    
    /**
     * @brief Send data to SPI device (half-duplex write).
     * 
     * @param buffer Source buffer containing data to transmit
     * @param length Number of bytes to transmit
     * 
     * @return 0 on success, negative error code on failure
     * @retval -1 SPI device not connected
     * @retval -2 ioctl failed
     */
    int8_t send_data(const uint8_t* buffer, uint8_t length) override;
    
    /**
     * @brief Flush any pending SPI operations.
     * Currently a no-op but reserved for future buffering implementations.
     */
    void flush() override;
    
    /**
     * @brief Full-duplex SPI transfer (simultaneous transmit and receive).
     * 
     * Transmits @p tx_buffer and simultaneously receives data into @p rx_buffer.
     * This is an atomic operation at the SPI protocol level.
     * 
     * @param tx_buffer Source buffer to transmit 
     * @param rx_buffer Destination buffer for received data 
     * @param length    Number of bytes to transfer
     * 
     * @return 0 on success, negative error code on failure
     * @retval -1 SPI device not connected
     * @retval -2 ioctl failed
     */
    int8_t transfer(uint8_t const* tx_buffer, uint8_t const* rx_buffer, const uint8_t length) override;

private:
    int spi_fd{-1};                          ///< File descriptor for the SPI device
    const char* device{"/dev/spidev1.0"};   ///< SPI device path (e.g., "/dev/spidev1.0")
    uint8_t mode{0};                         ///< SPI mode (0-3)
    uint8_t bits{8};                         ///< Bits per word
    uint32_t speed{18000000};                ///< Clock speed in Hz
    uint16_t delay{0};                       ///< Delay between transfers in microseconds
    
    /// Pre-allocated transfer structure for optimal performance (avoids malloc on each transfer)
    struct spi_ioc_transfer transfer_template;
};