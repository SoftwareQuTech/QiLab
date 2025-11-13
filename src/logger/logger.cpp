#include <fstream>
#include <mutex>

#include "logger/logger.hpp"

LocalProfiler::LocalProfiler() : initial_time_(std::chrono::steady_clock::now())
{
    events_.reserve(initial_number_of_logs_);
    durations_.reserve(initial_number_of_logs_);
}

void LocalProfiler::log_event(std::string_view message)
{
    using namespace std::chrono;
    const auto t = duration_cast<nanoseconds>(steady_clock::now() - initial_time_).count();

    std::lock_guard<std::mutex> lk(mu_);
    events_.emplace_back(message, t);
}

bool LocalProfiler::register_profiling_event(std::string_view event_id)
{
    if (event_id.empty())
        return false;
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lk(mu_);
    auto [it, inserted] = start_times_.emplace(event_id, now);
    if (!inserted)
    {
        // already present; refresh to now (or return false if you prefer)
        it->second = now;
    }
    return true;
}

bool LocalProfiler::pe_start(std::string_view event_id)
{
    if (event_id.empty())
        return false;
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lk(mu_);
    start_times_[event_id] = now; // auto-registers if absent
    return true;
}

bool LocalProfiler::pe_stop(std::string_view event_id)
{
    if (event_id.empty())
        return false;

    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lk(mu_);
    auto it = start_times_.find(event_id);
    if (it == start_times_.end())
        return false;

    const auto dur_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now - it->second).count();

    durations_.emplace_back(event_id, dur_ns);
    return true;
}

// Writes two CSVs:
//   <base_path>_events.csv:    "Message,TimestampNs"
//   <base_path>_durations.csv: "EventID,DurationNs"
void LocalProfiler::save_to_file(const std::string &base_path)
{
    std::lock_guard<std::mutex> lk(mu_);

    // events
    {
        std::ofstream ofs(base_path + "_events.csv");
        ofs << "Message,TimestampNs\n";
        for (const auto &event : events_)
        {
            ofs << event.first << "," << event.second << "\n";
        }
    }
    // durations
    {
        std::ofstream ofs(base_path + "_durations.csv");
        ofs << "EventID,DurationNs\n";
        for (const auto &duration: durations_)
        {
            ofs << duration.first << "," << duration.second << "\n";
        }
    }
}