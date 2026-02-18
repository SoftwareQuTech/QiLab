/*
** Created on: 18.11.2025
** Created by: mrkolev
*/

#pragma once

#if LOGGING_ENABLED

#include <string>
#include <mutex>
#include <string_view>
#include <atomic>
class StandardLogger
{
public:
    static StandardLogger &instance();

    void enable();
    void disable();
    void log_info(const std::string_view message);
    void log_warning(const std::string_view message);
    void log_error(const std::string_view message);

private:
    StandardLogger();
    
    std::atomic<bool> enabled_ = true;
    std::mutex logging_lock_;
    std::string get_timestamp();

    StandardLogger(const StandardLogger&) = delete;
    StandardLogger(const StandardLogger&&) = delete;
    StandardLogger& operator=(StandardLogger&) = delete;
    StandardLogger& operator=(StandardLogger&&) = delete;
};

// //RAII style logger
// struct LoggerGuard
// {
//     explicit LoggerGuard() = default;
//     ~LoggerGuard() = default;
// };

#endif // LOGGING_ENABLED