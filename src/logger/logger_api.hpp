#pragma once
#if LOGGING_ENABLED

#include "logger.hpp"

#define LOG_INFO(msg_sv) ::StandardLogger::instance().log_info(msg_sv)
#define LOG_WARNING(msg_sv) ::StandardLogger::instance().log_warning(msg_sv)
#define LOG_ERROR(msg_sv) ::StandardLogger::instance().log_error(msg_sv)
#define LOG_LEVEL(level) ::StandardLogger::instance().log_level(level)

#else // LOGGING_ENABLED == 0  -----------------------------
// No-ops when disabled; zero overhead, zero linking.
#define LOG_INFO(msg_sv) \
    do                   \
    {                    \
    } while (0)
#define LOG_WARNING(msg_sv) \
    do                      \
    {                       \
    } while (0)
#define LOG_ERROR(msg_sv) \
    do                    \
    {                     \
    } while (0)
#define LOG_LEVEL(level) \
    do                   \
    {                    \
    } while (0) 

#endif // LOGGING_ENABLED