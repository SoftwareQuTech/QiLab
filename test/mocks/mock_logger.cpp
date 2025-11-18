#include "logger/logger.hpp"
#include <string_view>
#include <mutex>

// No-op logger for tests
StandardLogger::StandardLogger() {}

StandardLogger& StandardLogger::instance()
{
    static StandardLogger instance;
    return instance;
}

void StandardLogger::log_info(const std::string_view) {
    // No-op
}

void StandardLogger::log_warning(const std::string_view) {
    // No-op
}

void StandardLogger::log_error(const std::string_view) {
    // No-op
}

std::string StandardLogger::get_timestamp() {
    return "";
}