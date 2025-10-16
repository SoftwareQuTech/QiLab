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