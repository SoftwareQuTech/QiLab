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
#include <unordered_map>

/**
 * @file
 * @brief Lightweight in-process profiling API
 *
 * This header defines a minimal profiling interface (#IProfiler) and a
 * thread-safe, in-memory implementation (#LocalProfiler). The profiler records:
 *  - Arbitrary log events with a relative timestamp (ns since construction).
 *  - Paired duration measurements via start/stop markers keyed by `event_id`.
 *
 * Data can be persisted to CSV files via save_to_file().
 *
 * @par Thread-safety
 * All public methods of #LocalProfiler are **thread-safe**. Internal state is
 * protected by a mutex. #IProfiler documents the expected semantics.
 *
 * @par Time base
 * All timestamps are relative to the #LocalProfiler construction time and use
 * `std::chrono::steady_clock` to avoid wall-clock adjustments.
 *
 * @par Example
 * @code{.cpp}
 * LocalProfiler prof;
 * prof.log_event("app.start");
 * 
 * prof.register_profiling_event("init")
 * // Scoped duration
 * {
 *     prof.pe_start("init");
 *     do_init();
 *     prof.pe_stop("init");
 * }
 *
 * // Arbitrary note
 * prof.log_event("ready");
 *
 * // Persist (will write <base>_events.csv and <base>_durations.csv)
 * prof.save_to_file("/tmp/qnode_profile");
 * @endcode
 */

/// @brief Abstract profiling interface for logging events and durations.
struct IProfiler
{
    /// Virtual destructor for interface.
    virtual ~IProfiler() = default;

    /**
     * @brief Record a free-form log event with a relative timestamp.
     *
     * Implementations should store @p message together with the number of
     * nanoseconds elapsed since profiler construction.
     *
     * @param message Arbitrary label for the event; may be empty.
     * @thread_safety Thread-safe if the concrete implementation is.
     */
    virtual void log_event(std::string_view message) = 0;

    /**
     * @brief Register (or hint) an event identifier intended for timing.
     *
     * Implementations may use this to pre-allocate or mark a key. Re-registering
     * an existing key should be harmless and typically returns `true`.
     *
     * @param event_id Non-empty identifier for a timed region.
     * @return `true` on success; `false` if @p event_id is empty or rejected.
     * @thread_safety Thread-safe iff the concrete implementation is.
     */
    virtual bool register_profiling_event(std::string_view event_id) = 0;

    /**
     * @brief Start timing a region identified by @p event_id.
     *
     * Semantics: records the current time as the start point for @p event_id.
     * If the identifier does not exist, implementations may implicitly create it.
     *
     * @param event_id Non-empty identifier for the region to start.
     * @return `true` on success; `false` if the input is invalid.
     * @thread_safety Thread-safe iff the concrete implementation is.
     */
    virtual bool pe_start(std::string_view event_id) = 0;

    /**
     * @brief Stop timing a region identified by @p event_id and record duration.
     *
     * Semantics: computes `now - start_time(event_id)` and stores the duration
     * in nanoseconds. If no matching start has been recorded, returns `false`.
     *
     * @param event_id Identifier previously passed to pe_start().
     * @return `true` if a duration was recorded; `false` otherwise.
     * @thread_safety Thread-safe iff the concrete implementation is.
     */
    virtual bool pe_stop(std::string_view event_id) = 0;

    /**
     * @brief Persist recorded data to CSV files.
     *
     * Implementations are expected to write two files:
     *  - `<base_path>_events.csv`    with columns: `Message,TimestampNs`
     *  - `<base_path>_durations.csv` with columns: `EventID,DurationNs`
     *
     * @param base_path Path prefix (without extension) for output files.
     * @thread_safety Thread-safe iff the concrete implementation is.
     */
    virtual void save_to_file(const std::string& base_path) = 0;
};

/**
 * @brief Thread-safe in-memory profiler with CSV persistence.
 *
 * #LocalProfiler stores:
 *  - Free-form events as `(message, timestamp_ns)`.
 *  - Durations as `(event_id, duration_ns)` collected via pe_start()/pe_stop().
 *
 * All state is protected by a mutex, making calls safe from multiple threads.
 * Timestamps are relative to construction time and use steady_clock.
 *
 * @note Copy and move operations are deleted to preserve internal invariants.
 */
class LocalProfiler final: public IProfiler
{
public:
    /**
     * @brief Construct a new LocalProfiler and capture the reference time point.
     *
     * Reserves internal buffers to reduce early reallocations.
     */
    LocalProfiler();

    /// @brief Defaulted virtual destructor.
    ~LocalProfiler() override = default;

    /**
     * @copydoc IProfiler::log_event()
     *
     * @warning Large volumes of events may increase contention on the internal
     * mutex; consider batching or sampling for very hot paths.
     */
    void log_event(std::string_view message = "") override;

    /**
     * @copydoc IProfiler::register_profiling_event()
     *
     * @note Calling this is optional; pe_start() will implicitly create keys.
     */
    bool register_profiling_event(std::string_view event_id) override;

    /**
     * @copydoc IProfiler::pe_start()
     *
     * @post A subsequent pe_stop(event_id) will emit a duration sample.
     */
    bool pe_start(std::string_view event_id) override;

    /**
     * @copydoc IProfiler::pe_stop()
     *
     * @post On success, a `(event_id, duration_ns)` sample is appended.
     */
    bool pe_stop(std::string_view event_id) override;

    /**
     * @copydoc IProfiler::save_to_file()
     *
     * @throws std::ios_base::failure on I/O errors (implementation-dependent).
     */
    void save_to_file(const std::string &base_path) override;

private:
    /**
     * @brief Initial reservation size for internal vectors.
     *
     * Used to reduce reallocations during early profiling.
     */
    const size_t initial_number_of_logs_ = 1000;

    /// @brief Reference time point for relative timestamps (steady_clock).
    std::chrono::steady_clock::time_point initial_time_;

    /// @name Protected by #mu_
    ///@{

    /// @brief Free-form events: `(message, timestamp_ns)`.
    std::vector<std::pair<std::string_view, long long>> events_;

    /// @brief Duration samples: `(event_id, duration_ns)`.
    std::vector<std::pair<std::string_view, long long>> durations_;

    /// @brief Start times per event id (steady_clock time points).
    std::unordered_map<std::string_view, std::chrono::steady_clock::time_point> start_times_;

    ///@}

    /// @brief Mutex guarding all mutable state.
    std::mutex mu_;

    // Non-copyable / non-movable (keeps references/mutex sane)
    LocalProfiler(LocalProfiler&&) = delete;                  ///< Deleted move ctor.
    LocalProfiler(const LocalProfiler&) = delete;             ///< Deleted copy ctor.
    LocalProfiler& operator=(LocalProfiler&&) = delete;       ///< Deleted move assign.
    LocalProfiler& operator=(const LocalProfiler&) = delete;  ///< Deleted copy assign.
};
