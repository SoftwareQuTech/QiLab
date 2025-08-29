/*
** Created on: 21.08.2025
** Created by: mrkolev
** Description: Contains all the response information for QNodeOS
*/

#pragma once
#include <string>
#include <chrono>
#include <vector>
#include <mutex>

struct ILogger
{
    virtual ~ILogger() = default;
    virtual void log_event(const std::string &event_id, const std::string &message = "") = 0;
    virtual void save_to_file(const std::string &path) = 0;
};

class LocalLogger: public ILogger
{
    public:
        LocalLogger();
        ~LocalLogger() = default;

        void log_event(const std::string &event_id, const std::string &message = "") override;
        void save_to_file(const std::string &path) override;

    private:
        std::chrono::time_point<std::chrono::high_resolution_clock> initial_time;       
        std::vector<std::string> logged_items{100}; //init with size so we get a deterministic logging at least for those logs
        std::string path_to_save;
        size_t log_num = 0;
        std::mutex data_lock{};

        LocalLogger(const LocalLogger&&) = delete;
        LocalLogger(const LocalLogger&) = delete;
        LocalLogger& operator=(LocalLogger&) = delete;
        LocalLogger& operator=(LocalLogger&&) = delete;        
};