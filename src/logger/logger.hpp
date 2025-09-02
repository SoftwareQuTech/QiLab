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

/**
 * @file
 * @brief Logging interfaces and a local in-memory logger for QNodeOS.
 *
 * This header defines a minimal logging abstraction (`ILogger`) and a concrete
 * single-process implementation (`LocalLogger`) that accumulates log events in
 * memory and can persist them to disk on demand.
 *
 * @section thread_safety Thread safety
 * - `LocalLogger` is thread-safe for concurrent calls to `log_event()` and
 *   `save_to_file()`; it uses an internal `std::mutex` to serialize access to
 *   shared state (`logged_items`, `log_num`, and `path_to_save`).
 * - Callers do not need to provide external synchronization.
 *
 * @section time_base Time base
 * - Timestamps are measured relative to construction time (`initial_time`)
 *   using `std::chrono::steady_clock`.
 */

/**
 * @brief Minimal logging interface.
 *
 * Implementations should be safe to call from multiple threads and may choose
 * to buffer messages in memory, write-through to disk, or forward to a remote
 * sink.
 */
struct ILogger
{
    virtual ~ILogger() = default;

    /**
     * @brief Record a log event.
     *
     * Implementations should attach a timestamp and append the formatted entry
     * to their internal storage. The \p event_id is expected to be a short,
     * machine-readable token (e.g., "BOOT_OK", "COMM_TIMEOUT"), while
     * \p message can carry free-form human-readable details.
     *
     * @param event_id   Identifier for the event category or code.
     * @param message    Optional, additional context for the event.
     */
    virtual void log_event(const std::string &event_id,
                           const std::string &message = "") = 0;

    /**
     * @brief Persist accumulated log entries to a file.
     *
     * The path is implementation-defined; implementations may also cache the
     * last used path to allow subsequent saves without specifying \p path again.
     *
     * @param path   Filesystem path to write to (created or truncated).
     */
    virtual void save_to_file(const std::string &path) = 0;
};

/**
 * @brief In-memory logger with optional persistence to a local file.
 *
 * `LocalLogger` stores formatted log lines in a `std::vector<std::string>` and
 * protects all shared state with a mutex, making it safe for use from multiple
 * threads. It pre-sizes its storage to initial_number_of_logs elements to reduce early
 * reallocations and provide deterministic capacity for the first initial_number_of_logs.
 *
 * @note Copy and move operations are explicitly deleted; each logger instance
 *       uniquely owns its internal buffer and state.
 */
class LocalLogger: public ILogger
{
    public:
        /**
         * @brief Construct a new `LocalLogger`.
         *
         * Initializes the reference time point used to compute relative
         * timestamps for subsequent log entries.
         */
        LocalLogger();

        /// @brief Defaulted virtual destructor.
        ~LocalLogger() = default;

        /**
         * @copydoc ILogger::log_event()
         *
         * @details
         * Format is implementation-defined but typically includes:
         * `<event_id>, <message>, [+<ns_since_start>]`.
         *
         * @warning If the internal buffer reaches capacity and reallocation
         *          occurs, this function may incur additional latency.
         */
        void log_event(const std::string &event_id,
                       const std::string &message = "") override;

        /**
         * @copydoc ILogger::save_to_file()
         *
         * @details
         * On success, the path may be cached internally to allow later saves
         * to the same location without updating \c path_to_save explicitly.
         *
         * @throws std::runtime_error or similar if the path cannot be opened
         *         or the write fails (implementation-defined).
         */
        void save_to_file(const std::string &path) override;

    private:
        const uint16_t initial_number_of_logs = 1000;
        /**
         * @brief Reference time used to generate relative timestamps.
         *
         * Set during construction and never mutated afterwards.
         */
        std::chrono::time_point<std::chrono::steady_clock> initial_time;

        /**
         * @brief In-memory collection of formatted log lines.
         *
         * Pre-sized to 100 entries to reduce early reallocations and make
         * early-stage logging performance more deterministic.
         */
        std::vector<std::string> logged_items{initial_number_of_logs};

        /**
         * @brief Number of log entries currently stored.
         *
         * Used to track insertion position when the vector is pre-sized.
         */
        size_t log_num = 0;

        /**
         * @brief Mutex protecting access to buffer and counters.
         *
         * Guards `logged_items`, `log_num`, and `path_to_save`.
         */
        std::mutex data_lock{};

        // Non-copyable and non-movable to preserve unique ownership semantics.
        LocalLogger(LocalLogger&&) = delete;   ///< Deleted move constructor.
        LocalLogger(LocalLogger&) = delete;    ///< Deleted copy constructor.
        LocalLogger& operator=(LocalLogger&) = delete;      ///< Deleted copy assignment.
        LocalLogger& operator=(LocalLogger&&) = delete;     ///< Deleted move assignment.
};
