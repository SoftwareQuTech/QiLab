/*
** Created on: 25.08.2025
** Created by: mrkolev
** Description: Contains the implementation for a mock command handler.
**              The idea is fixed commands can be send in an uniform program
*/

#pragma once
#include "CommandHandler/transmision_handler.hpp"
#include "logger/logger.hpp"

struct BaseC
{
    #define CRC8_CCITT_POLY 0x07u

    virtual void create_command(uint8_t *buff) = 0;
    //some crc calculation that is used for everyone
    inline uint8_t crc8_ccitt_update(uint8_t crc, uint8_t byte)
    {
        crc ^= byte;
        for (uint8_t i = 0; i < 8; ++i)
        {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ CRC8_CCITT_POLY)
                               : (uint8_t)(crc << 1);
        }
        return crc;
    }

    uint8_t crc8_ccitt(const uint8_t *data, size_t len)
    {
        const uint8_t *p = (const uint8_t *)data;
        uint8_t crc = 0x00u; // init
        while (len--)
        {
            crc = crc8_ccitt_update(crc, *p++);
        }
        return crc; // xorout 0x00
    }
};

class MockHandler final : public ITransmissionHandler
{
public:
    MockHandler();
    MockHandler(ILogger*);
    ~MockHandler();
    uint8_t receive_data(uint8_t *, uint8_t) override;
    uint8_t send_data(const uint8_t *, uint8_t) override;

private:
    void fill_mock_message(uint8_t*);
    void create_commands();
    ILogger* local_logger;
    std::vector<BaseC*> command_list;
    MockHandler(const MockHandler &) = delete;
    MockHandler(const MockHandler &&) = delete;
    MockHandler &operator=(MockHandler &) = delete;
    MockHandler &operator=(MockHandler &&) = delete;
};