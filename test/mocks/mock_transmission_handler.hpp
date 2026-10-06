// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

#pragma once

#include <gmock/gmock.h>

#include "DataTransmission/transmision_handler.hpp"

class MockTransmissionHandler : public ITransmissionHandler {
public:
    MOCK_METHOD(int8_t,  send_data,    (const uint8_t* data, uint8_t len), (override));
    MOCK_METHOD(int8_t,  receive_data, (uint8_t* data, uint8_t len),       (override));
    MOCK_METHOD(int8_t,  transfer, (uint8_t const* tx, uint8_t const* rx, const uint8_t len),       (override));
    MOCK_METHOD(void,  flush, (),       (override));
};
