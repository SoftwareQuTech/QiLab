/*
** Created on: 21.08.2025
** Created by: mrkolev
** Description: Contains all the response information for QNodeOS
*/

#pragma once
#include <string>

class ILogger
{
    virtual ~ILogger();
    virtual void log_event(const std::string &event) = 0;
    virtual void save_to_file(const std::string &path) = 0;
};

class LocalLogger: public ILogger
{
    public:
        LocalLogger();
        ~LocalLogger();
        void log_event(const std::string &event) override;
        void save_to_file(const std::string &path) override;
    private:
        std::string path_to_save;

        LocalLogger(const LocalLogger&&) = delete;
        LocalLogger(const LocalLogger&) = delete;
        LocalLogger& operator=(LocalLogger&) = delete;
        LocalLogger& operator=(LocalLogger&&) = delete;        
};