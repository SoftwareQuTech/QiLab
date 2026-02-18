#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

#include "logger/logger.hpp"

StandardLogger::StandardLogger() {}

void StandardLogger::enable()
{
    enabled_ = true;
}
void StandardLogger::disable()
{
    enabled_ = false;
}

StandardLogger& StandardLogger::instance()
{
    static StandardLogger instance;
    return instance;
}

std::string StandardLogger::get_timestamp()
{
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_time_t), "%H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << now_ms.count();
    
    return ss.str();
}

void StandardLogger::log_info(const std::string_view message)
{
    if (!enabled_.load(std::memory_order_relaxed)) return;
    std::lock_guard<std::mutex> lock(logging_lock_);
    std::clog << "[" << get_timestamp() << "] [INFO]: " << message << std::endl;
}
void StandardLogger::log_warning(const std::string_view message)
{
    if (!enabled_.load(std::memory_order_relaxed)) return;
    std::lock_guard<std::mutex> lock(logging_lock_);
    std::clog << "[" << get_timestamp() << "] [WARNING]: " << message << std::endl;
}
void StandardLogger::log_error(const std::string_view message)
{
    if (!enabled_.load(std::memory_order_relaxed)) return;
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