// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

/*
** Created on: 21.08.2025
** Created by: mrkolev
** Description: Contains all the commands information for QNodeOS
*/

#pragma once
#include <cstdint>
#include <functional>
#include <memory>

#include "DataTypes/responses.hpp"
#include "DataTypes/commands.hpp"

struct CommandData_t
{
    std::function<void(const ResponseData&)> response_callback;
    std::unique_ptr<CommandBase> command;   // OWN it here
    uint16_t node_id;
};