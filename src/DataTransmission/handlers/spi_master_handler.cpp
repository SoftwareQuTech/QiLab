#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <cstring>
#include <iostream>

#include "logger/profiler_api.hpp"
#include "DataTransmission/handlers/spi_master_handler.hpp"


SPIHandler::SPIHandler(){}

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
        //keep this for debugging purposes only
        std::cerr << "Failed to open SPI device: " << device << std::endl;
        return -1;
    }

    if (ioctl(spi_fd, SPI_IOC_WR_MODE, &mode) == -1) {
        //keep this for debugging purposes only
        std::cerr << "Failed to set SPI mode." << std::endl;
        close(spi_fd);
        return -1;
    }

    if (ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) == -1) {
        //keep this for debugging purposes only
        std::cerr << "Failed to set bits per word." << std::endl;
        close(spi_fd);
        return -1;
    }

    if (ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) == -1) {
        //keep this for debugging purposes only
        std::cerr << "Failed to set max speed." << std::endl;
        close(spi_fd);
        return -1;
    }

    return 0;
}

int8_t SPIHandler::receive_data(uint8_t* buffer, uint8_t length)
{
    if (read(spi_fd, buffer, length) != length) {
        //keep this for debugging purposes only
        std::cerr << "Failed to read from SPI device." << std::endl;
        return -1;
    }
    return 0;
}

int8_t SPIHandler::send_data(const uint8_t* buffer, uint8_t length)
{
    if (write(spi_fd, buffer, length) != length) {
        //keep this for debugging purposes only
        std::cerr << "Failed to write to SPI device." << std::endl;
        return -1;
    }
    return 0;
}

int8_t SPIHandler::transfer(uint8_t const* tx_buffer, uint8_t const* rx_buffer, const uint8_t length)
{
    struct spi_ioc_transfer tr;
    memset(&tr, 0, sizeof(tr));
    tr.tx_buf = reinterpret_cast<unsigned long>(tx_buffer);
    tr.rx_buf = reinterpret_cast<unsigned long>(const_cast<uint8_t*>(rx_buffer));
    tr.len = length;
    tr.delay_usecs = delay;
    tr.speed_hz = speed;
    tr.bits_per_word = bits;

    if (ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr) < 1) {
        //keep this for debugging purposes only
        std::cerr << "Failed to perform SPI transfer." << std::endl;
        return -1;
    }
    return 0;
}

void SPIHandler::flush()
{
    // No specific flush operation for SPI; implement if needed.
}