/*
** Created on: 16.10.2025
** Created by: mrkolev
** Description: Contains a handler for SPI master communication.
*/

#pragma once
#include <memory>
#include <vector>
#include <cstdint>

#include "DataTransmission/transmision_handler.hpp"

class SPIHandler: public ITransmissionHandler
{
public:
    SPIHandler();
    ~SPIHandler() override;
    int8_t connect_spi(const char* device_path = "/dev/spidev0.0", uint8_t spi_mode = 0, uint8_t bits_per_word = 8, uint32_t max_speed_hz = 500000);
    int8_t receive_data(uint8_t* buffer, uint8_t length) override;
    int8_t send_data(const uint8_t* buffer, uint8_t length) override;
    void flush() override;
    int8_t transfer(uint8_t const* tx_buffer, uint8_t const* rx_buffer, const uint8_t length) override;
private:
    int spi_fd;
    const char* device = "/dev/spidev0.0";
    uint8_t mode = 0;
    uint8_t bits = 8;
    uint32_t speed = 500000;
    uint16_t delay = 0;
};