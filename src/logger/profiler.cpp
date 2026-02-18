#include "profiler.hpp"
#if PROF_ENABLED

#include <fstream>
#include <algorithm>
#include <filesystem>
#include <cstdlib>

static std::string resolve_profiler_base_path(const std::string& base_path) {
    namespace fs = std::filesystem;

    fs::path base = base_path.empty() ? fs::path("profile") : fs::path(base_path);

    // Absolute paths are always honored as-is.
    if (base.is_absolute()) {
        return base.string();
    }

    // Highest priority: explicit full base path override.
    const char* explicit_base = std::getenv("QILAB_PROFILER_PATH");
    if (explicit_base && explicit_base[0] != '\0') {
        return std::string(explicit_base);
    }

    // Next priority: profiler output directory override.
    const char* profiler_dir = std::getenv("QILAB_PROFILER_DIR");
    if (profiler_dir && profiler_dir[0] != '\0') {
        return (fs::path(profiler_dir) / base).string();
    }

    // Default behavior: use provided relative base path.
    return base.string();
}

struct EventData
{
    std::int64_t max_runtime;
    std::int64_t min_runtime;
    std::uint64_t event_count;
    std::int64_t event_duration_average;
};

// ----------------- Private TLS class -----------------
class ProfilerLocalStorage {
public:
    ProfilerLocalStorage() = default;

    void init_local_storage(std::size_t max_events, std::size_t reserve_per_event) 
    {
        //reserve_per_event_ = reserve_per_event;
        start_time_event_.assign(max_events, {});
        event_data.resize(max_events);
        for(auto& e: event_data)
        {
            e.event_count = 0;
            e.event_duration_average = 0;
            e.max_runtime = 0;
            e.min_runtime = 0;
        }
        // samples_ns_.clear();
        // samples_ns_.resize(max_events);
        // for (auto& sample : samples_ns_) 
        // {
        //     sample.reserve(reserve_per_event_);
        // }
    }

    void ensure_capacity(std::size_t need_events) 
    {
        if (start_time_event_.size() >= need_events) 
            return;
        const std::size_t old = start_time_event_.size();
        start_time_event_.resize(need_events);
        event_data.resize(need_events);
        //samples_ns_.resize(need_events);
        //for (std::size_t i = old; i < need_events; ++i)
            //samples_ns_[i].reserve(reserve_per_event_);
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
        // samples_ns_[event_id].push_back(static_cast<std::int64_t>(ns));
        auto *data = &event_data[event_id];
        if(data->max_runtime < ns)
        {
            data->max_runtime = ns;
        }
        if(data->min_runtime > ns || data->event_count == 0) //just for the first one
        {
            data->min_runtime = ns;
        }
        data->event_count++;

    }

    void merge_samples(std::vector<EventData>& common_container) 
    {
        if (common_container.size() < event_data.size()) 
        {
            common_container.resize(event_data.size());
        }

        // Merge the data by updating existing entries
        for (size_t i = 0; i < event_data.size(); ++i) 
        {
            // Take the larger max_runtime
            if (event_data[i].max_runtime > common_container[i].max_runtime) {
                common_container[i].max_runtime = event_data[i].max_runtime;
            }

            // Take the smaller min_runtime (if not zero)
            if (event_data[i].min_runtime != 0 && 
                (common_container[i].min_runtime == 0 || 
                 event_data[i].min_runtime < common_container[i].min_runtime)) {
                common_container[i].min_runtime = event_data[i].min_runtime;
            }

            // Accumulate the event count
            common_container[i].event_count += event_data[i].event_count;
            
            // Update average duration if needed
            if (event_data[i].event_count > 0) {
                common_container[i].event_duration_average = 
                    (common_container[i].event_duration_average + event_data[i].event_duration_average) / 2;
            }
        }

        event_data.clear();
    }

private:
    //the start time per event basis
    std::vector<std::chrono::steady_clock::time_point> start_time_event_;
    //event data 
    std::vector<EventData>                              event_data;
    //the samples in [ns] for each event
    //std::vector<std::vector<std::int64_t>>             samples_ns_;
    //reservetion for events for each event
    //std::size_t                                        reserve_per_event_{1u << 15};
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
    const std::string resolved_base_path = resolve_profiler_base_path(base_path);
    const std::filesystem::path resolved_base{resolved_base_path};
    const auto parent_dir = resolved_base.parent_path();
    if (!parent_dir.empty()) {
        std::filesystem::create_directories(parent_dir);
    }

    // durations
    {
        std::lock_guard<std::mutex> lk(merge_lock_);
        std::ofstream ofs(resolved_base_path + "_durations.csv");
        ofs << "Event,Max[ns],Min[ns],Count\n";
        const std::size_t n = std::min(common_container_.size(), names_.size());
        for (std::size_t id = 0; id < n; ++id) {
            const std::string& name = names_[id];
            ofs << name << "," ;
            ofs << std::to_string(common_container_[id].max_runtime) << ",";
            ofs << std::to_string(common_container_[id].min_runtime) << ",";
            ofs << std::to_string(common_container_[id].event_count) << "\n";

            // for (const auto ns : common_container_[id]) {
            //     ofs << name << "," << ns << "\n";
            // }
        }
        ofs.flush();
    }
    // free-form messages
    {
        std::lock_guard<std::mutex> lk(events_lock_);
        std::ofstream ofs(resolved_base_path + "_events.csv");
        ofs << "Message,TimestampNs\n";
        for (const auto& e : events_) ofs << e.first << "," << e.second << "\n";
        ofs.flush();
    }
}

std::size_t Profiler::event_count() const {
    std::lock_guard<std::mutex> lk(registration_lock_);
    return max_events_;
}

// RAII
ProfGuard::ProfGuard(std::size_t reserve_per_event) {
    Profiler::instance().thread_init(reserve_per_event);
}
ProfGuard::~ProfGuard() {
    Profiler::instance().thread_merge_data();
}

#endif // PROF_ENABLED
