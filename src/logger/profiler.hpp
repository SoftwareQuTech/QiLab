#pragma once
#include "profiler_config.hpp"

#if PROF_ENABLED

#include <vector>
#include <string>
#include <string_view>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <unordered_map>

class ProfilerLocalStorage;
struct EventData;

class Profiler
{
public:
    static Profiler &instance();

    std::uint16_t register_event(std::string_view name);
    std::uint16_t register_event_literal(const char *name, std::size_t len);

    void thread_init(std::size_t reserve_per_event = (1u << 15));

    void start(std::uint16_t event_id) noexcept; // hot path
    void stop(std::uint16_t event_id) noexcept;  // hot path

    void thread_merge_data(); // call on the SAME thread

    void log_event(std::string_view message); // free-form (mutexed, cold)

    void dump_csv(const std::string &base_path); // call after all threads flushed/joined

    std::size_t event_count() const;

private:
    Profiler();
    Profiler(const Profiler &) = delete;
    Profiler &operator=(const Profiler &) = delete;

    // Registry (cold path)
    mutable std::mutex registration_lock_;
    std::vector<std::string> names_;
    std::unordered_map<std::string, uint16_t> name_to_id_;
    std::size_t max_events_{0};

    // Sink for durations (merge at flush/dump)
    std::mutex merge_lock_;
    //std::vector<std::vector<std::int64_t>> common_container_; // [event_id] -> durations (ns)
    std::vector<EventData> common_container_;

    // Free-form messages (not hot path)
    std::mutex events_lock_;
    std::chrono::steady_clock::time_point start_time_;
    std::vector<std::pair<std::string, std::int64_t>> events_;

    // TLS (hot path) – hidden definition in profiler.cpp
    static thread_local ProfilerLocalStorage local_storage_;
};

struct ProfGuard {
    explicit ProfGuard(std::size_t reserve_per_event = (1u << 15));
    ~ProfGuard();
};

#endif // PROF_ENABLED
