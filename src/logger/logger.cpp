#include <fstream>
#include <mutex>

#include "logger/logger.hpp"
LocalLogger::LocalLogger()
{
    initial_time = std::chrono::high_resolution_clock::now();
}

void LocalLogger::log_event(const std::string &event_id, const std::string &message)
{
    auto duration = std::chrono::high_resolution_clock::now() - initial_time;
    auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
    auto mess = event_id + "," + message + "," + std::to_string(timestamp.count());
    
    if (log_num == logged_items.capacity())
    {
        logged_items.resize(log_num + 100);
    }
    data_lock.lock();
    logged_items[log_num++] = mess;
    data_lock.unlock();
}

void LocalLogger::save_to_file(const std::string &path)
{
    std::ofstream file_to_write(path);
    file_to_write << "Event ID, Message, Timestamp" << std::endl;
    for (auto item : logged_items)
    {
        file_to_write << item << std::endl;
    }
    file_to_write.close();
}