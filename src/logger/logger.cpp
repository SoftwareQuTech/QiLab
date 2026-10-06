// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

#include "logger/logger.hpp"

StandardLogger::StandardLogger() {}

void StandardLogger::log_level(const std::string_view level)
{
    std::lock_guard<std::mutex> lock(logging_lock_);
    if (level == "DEBUG")
    {
        debug_level.store(true, std::memory_order_relaxed);
        error_level.store(true, std::memory_order_relaxed);
    }
    else if (level == "ERROR")
    {
        debug_level.store(false, std::memory_order_relaxed);
        error_level.store(true, std::memory_order_relaxed);
    }
    else if (level == "OFF")
    {
        debug_level.store(false, std::memory_order_relaxed);
        error_level.store(false, std::memory_order_relaxed);
    }
    else
    {
        std::clog << "Invalid log level: " << level << ". Valid levels are: DEBUG, ERROR, OFF." << std::endl;
        return;
    }
    std::clog << "[" << get_timestamp() << "] [ Log level set to: " << level << "]: " << std::endl;
}

StandardLogger &StandardLogger::instance()
{
    static StandardLogger instance;
    return instance;
}

std::string StandardLogger::get_timestamp()
{
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto now_ms = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()) % 1000000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_time_t), "%H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(6) << now_ms.count();
    
    return ss.str();
}

void StandardLogger::log_info(const std::string_view message)
{
    if (!debug_level.load(std::memory_order_relaxed)) return;
    std::lock_guard<std::mutex> lock(logging_lock_);
    std::clog << "[" << get_timestamp() << "] [INFO]: " << message << std::endl;
}
void StandardLogger::log_warning(const std::string_view message)
{
    if (!error_level.load(std::memory_order_relaxed)) return;
    std::lock_guard<std::mutex> lock(logging_lock_);
    std::clog << "[" << get_timestamp() << "] [WARNING]: " << message << std::endl;
}
void StandardLogger::log_error(const std::string_view message)
{
    if (!error_level.load(std::memory_order_relaxed)) return;
    std::lock_guard<std::mutex> lock(logging_lock_);
    std::cerr << "[" << get_timestamp() << "] [ERROR]: " << message << std::endl;
}

// LoggerGuard::LoggerGuard()
// {
//     StandardLogger::instance().log_info("LoggerGuard created");
// }
// LoggerGuard::~LoggerGuard()
// {
//     StandardLogger::instance().log_info("LoggerGuard destroyed");
// }