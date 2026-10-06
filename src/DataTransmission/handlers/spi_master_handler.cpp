// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <string>

#include "logger/profiler_api.hpp"
#include "logger/logger_api.hpp"
#include "DataTransmission/handlers/spi_master_handler.hpp"


SPIHandler::SPIHandler(){
    // Use default-initialized members to establish the connection
    connect_spi(device, mode, bits, speed);
}

SPIHandler::~SPIHandler()
{
    if(spi_fd >= 0)
    {
        close(spi_fd);
    }
}

int8_t SPIHandler::connect_spi(const char* device_path, uint8_t spi_mode, uint8_t bits_per_word, uint32_t max_speed_hz)
{
    device = device_path;
    mode = spi_mode;
    bits = bits_per_word;
    speed = max_speed_hz;

    spi_fd = open(device, O_RDWR);
    if (spi_fd < 0) {
        LOG_ERROR("Failed to open SPI device: " + std::string(device));
        return -1;
    }

    if (ioctl(spi_fd, SPI_IOC_WR_MODE, &mode) == -1) {
        LOG_ERROR("Failed to set SPI mode.");
        close(spi_fd);
        return -1;
    }

    if (ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) == -1) {
        LOG_ERROR("Failed to set bits per word.");
        close(spi_fd);
        return -1;
    }

    // Initialize transfer template
    memset(&transfer_template, 0, sizeof(transfer_template));
    transfer_template.speed_hz = speed;
    transfer_template.bits_per_word = bits;
    transfer_template.delay_usecs = 0;  // No delay between transfers

    if (ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) == -1) {
        LOG_ERROR("Failed to set max speed.");
        close(spi_fd);
        return -1;
    }

    return 0;
}

int8_t SPIHandler::receive_data(uint8_t* buffer, uint8_t length)
{
    if (read(spi_fd, buffer, length) != length) {
        LOG_ERROR("Failed to read from SPI device.");
        return -1;
    }
    return -1; //Not Implemented
}

int8_t SPIHandler::send_data(const uint8_t* buffer, uint8_t length)
{
    if (write(spi_fd, buffer, length) != length) {
        LOG_ERROR("Failed to write to SPI device.");
        return -1;
    }
    return -1; //Not Implemented
}

int8_t SPIHandler::transfer(uint8_t const* tx_buffer, uint8_t const* rx_buffer, const uint8_t length)
{
    // Use pre-initialized template for better performance
    struct spi_ioc_transfer tr = transfer_template;
    tr.tx_buf = reinterpret_cast<unsigned long>(tx_buffer);
    tr.rx_buf = reinterpret_cast<unsigned long>(const_cast<uint8_t*>(rx_buffer));
    tr.len = length;

    // Perform transfer with error checking
    int ret = ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
    if (ret != length) {
        LOG_ERROR("SPI transfer failed. Length is wrong");
        return -1;
    }
    
    return 0;
}

void SPIHandler::flush()
{
    // No specific flush operation for SPI; implement if needed.
    LOG_WARNING("SPI flush() called, but no operation is defined.");
}