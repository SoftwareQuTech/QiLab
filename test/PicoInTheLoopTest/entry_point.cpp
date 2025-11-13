#include <iostream>

#include "DataHandler/spi_master_handler.hpp"
#include "DataHandler/command_helper.hpp"


int main(void)
{
    SPIHandler spi_qilab;
    SPIHandler spi_qnodeos;
    std::cout << "Starting Loop testing" << std::endl;

    if(spi_qilab.connect_spi("/dev/spidev0.0", 0, 8, 1000000) < 0)
    {
        std::cerr << "Cannot connect to spi 0" << std::endl;
        return -1;
    }

    if(spi_qnodeos.connect_spi("/dev/spidev1.0", 0, 8, 1000000))
    {
        std::cerr << "Cannot connect to spi 1" << std::endl;
        return -1;
    }

    BaseC* sg_command = new SingleQGate(0, Angle_t::_15_PI_16, Axis_t::X_AXIS);
    BaseC* nop_command = new Nop();

    uint8_t tx_buff[4] = {0};
    uint8_t rx_buff[4] = {0};
    nop_command->create_command(tx_buff, 4);

    if(spi_qnodeos.transfer(tx_buff, rx_buff, 4) < -1)
    {
        std::cerr << "Cannot send command" << std::endl;
    }
    std::cout << "Received response: ";
    for(auto rx: rx_buff)
    {
        std::cout << std::hex << rx;
    }
    std::cout << std::endl;

    //create some response for it to read
    tx_buff[0] = 0x1;
    tx_buff[1] = 0x1;
    tx_buff[2] = 0x2;
    tx_buff[3] = 0x3;
    if(spi_qnodeos.transfer(tx_buff, rx_buff, 4) < -1)
    {
        std::cerr << "Cannot send command" << std::endl;
    }
    std::cout << "Received command: ";
    for(auto rx: rx_buff)
    {
        std::cout << std::hex << rx;
    }
    std::cout << std::endl;


    if(spi_qnodeos.transfer(tx_buff, rx_buff, 4) < -1)
    {
        std::cerr << "Cannot send command" << std::endl;
    }
    std::cout << "Received response: ";
    for(auto rx: rx_buff)
    {
        std::cout << std::hex << rx;
    }
    std::cout << std::endl;

    std::cout << "Ended Loop testing" << std::endl;
    return 0;
}