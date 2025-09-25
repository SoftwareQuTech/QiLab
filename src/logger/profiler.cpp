#include "profiler.hpp"

#if PROF_ENABLED

#include <fstream>
#include <algorithm>

// ----------------- Private TLS class -----------------
class ProfilerLocalStorage {
public:
    ProfilerLocalStorage() = default;

    void init_local_storage(std::size_t max_events, std::size_t reserve_per_event) 
    {
        reserve_per_event_ = reserve_per_event;
        start_time_event_.assign(max_events, {});
        samples_ns_.clear();
        samples_ns_.resize(max_events);
        for (auto& sample : samples_ns_) 
        {
            sample.reserve(reserve_per_event_);
        }
    }

    void ensure_capacity(std::size_t need_events) 
    {
        if (start_time_event_.size() >= need_events) 
            return;
        const std::size_t old = start_time_event_.size();
        start_time_event_.resize(need_events);
        samples_ns_.resize(need_events);
        for (std::size_t i = old; i < need_events; ++i)
            samples_ns_[i].reserve(reserve_per_event_);
    }

    inline void start(std::uint16_t event_id) 
    {
        if (event_id >= start_time_event_.size()) 
        {
            ensure_capacity(static_cast<std::size_t>(event_id) + 1);
        }
        start_time_event_[event_id] = std::chrono::steady_clock::now();
    }

    inline void stop(std::uint16_t event_id) 
    {
        if (event_id >= start_time_event_.size()) 
            return;
        const auto now = std::chrono::steady_clock::now();
        const auto ns  = std::chrono::duration_cast<std::chrono::nanoseconds>(now - start_time_event_[event_id]).count();
        samples_ns_[event_id].push_back(static_cast<std::int64_t>(ns));
    }

    void merge_samples(std::vector<std::vector<std::int64_t>>& common_container) 
    {
        if (common_container.size() < samples_ns_.size()) 
        {
            common_container.resize(samples_ns_.size());
        }
        for (std::size_t id = 0; id < samples_ns_.size(); ++id) {
            auto& dst = common_container[id];
            auto& src = samples_ns_[id];
            if (!src.empty()) {
                dst.insert(dst.end(),
                           std::make_move_iterator(src.begin()),
                           std::make_move_iterator(src.end()));
                src.clear();
            }
        }
    }

private:
    //the start time per event basis
    std::vector<std::chrono::steady_clock::time_point> start_time_event_;
    //the samples in [ns] for each event
    std::vector<std::vector<std::int64_t>>             samples_ns_;
    //reservetion for events for each event
    std::size_t                                        reserve_per_event_{1u << 15};
};

// ----------------- Prof definitions -----------------

thread_local ProfilerLocalStorage Profiler::local_storage_{};

Profiler::Profiler() {
    start_time_ = std::chrono::steady_clock::now();
}

Profiler& Profiler::instance() {
    static Profiler inst;
    return inst;
}

std::uint16_t Profiler::register_event(std::string_view name) 
{
    std::lock_guard<std::mutex> lk(registration_lock_);
    auto it = name_to_id_.find(std::string{name});

    if (it != name_to_id_.end()) 
        return it->second;

    const std::uint16_t id = static_cast<std::uint16_t>(names_.size());
    names_.emplace_back(name);                    // own storage
    name_to_id_.emplace(names_.back(), id);       // key refers to owned string
    max_events_ = names_.size();
    return id;
}

std::uint16_t Profiler::register_event_literal(const char* name, std::size_t len) 
{
    return register_event(std::string_view{name, len});
}

void Profiler::thread_init(std::size_t reserve_per_event) 
{
    std::size_t max_events_snapshot;
    { 
        std::lock_guard<std::mutex> lk(registration_lock_); 
        max_events_snapshot = max_events_; 
    }
    local_storage_.init_local_storage(max_events_snapshot, reserve_per_event);
}

void Profiler::start(std::uint16_t event_id) noexcept 
{
    local_storage_.start(event_id);
}

void Profiler::stop(std::uint16_t event_id) noexcept 
{
    local_storage_.stop(event_id);
}

void Profiler::thread_merge_data() {
    std::lock_guard<std::mutex> lk(merge_lock_);
    local_storage_.merge_samples(common_container_);
}

void Profiler::log_event(std::string_view message) {
    const auto now   = std::chrono::steady_clock::now();
    const auto ts_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now - start_time_).count();
    std::lock_guard<std::mutex> lk(events_lock_);
    events_.emplace_back(std::string{message}, static_cast<std::int64_t>(ts_ns));
}

void Profiler::dump_csv(const std::string& base_path) {
    // durations
    {
        std::lock_guard<std::mutex> lk(merge_lock_);
        std::ofstream ofs(base_path + "_durations.csv");
        ofs << "Event,DurationNs\n";
        const std::size_t n = std::min(common_container_.size(), names_.size());
        for (std::size_t id = 0; id < n; ++id) {
            const std::string& name = names_[id];
            for (const auto ns : common_container_[id]) {
                ofs << name << "," << ns << "\n";
            }
        }
        ofs.flush();
    }
    // free-form messages
    {
        std::lock_guard<std::mutex> lk(events_lock_);
        std::ofstream ofs(base_path + "_events.csv");
        ofs << "Message,TimestampNs\n";
        for (const auto& e : events_) ofs << e.first << "," << e.second << "\n";
        ofs.flush();
    }
}

std::size_t Profiler::event_count() const {
    std::lock_guard<std::mutex> lk(registration_lock_);
    return max_events_;
}

// // RAII
// ProfGuard::ProfGuard(std::size_t reserve_per_event) {
//     Profiler::instance().thread_init(reserve_per_event);
// }
// ProfGuard::~ProfGuard() {
//     Profiler::instance().thread_merge_data();
// }

#endif // PROF_ENABLED
