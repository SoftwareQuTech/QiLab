// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

/**
 * @file data_acquisition.hpp
 * @brief Structured data-acquisition sink for research/experiment data.
 *
 * The DataAcquisition component lets researchers record arbitrary, structured
 * data from arbitrary locations in the codebase to a dedicated file, separate
 * from the human-readable diagnostic StandardLogger. Each record is written as
 * one JSON object per line (JSONL), which is readable and trivially parsed for
 * offline analysis (e.g. pandas.read_json(path, lines=True)).
 *
 * Design mirrors the Profiler: a thread-safe singleton, a compile-time enable
 * flag (DAQ_ENABLED) that compiles the whole thing away when disabled, and the
 * same output directory (~/.qilab/logs/) so all instrument data lives in one
 * place. Data acquisition is additionally gated at runtime (configure()) so it
 * only writes when explicitly enabled.
 *
 * @note Not intended for direct use; use data_acquisition_api.hpp macros.
 * @note Not a hot path. For low-overhead timing use the Profiler instead.
 */

#pragma once

#if DAQ_ENABLED

#include <string>
#include <string_view>
#include <mutex>
#include <atomic>
#include <fstream>
#include <cstdint>

/**
 * @brief Thread-safe singleton sink that appends JSONL records to a file.
 *
 * @note Access via DataAcquisition::instance(). Use DataRecord to build records.
 */
class DataAcquisition
{
public:
    static DataAcquisition &instance();

    /**
     * @brief Enable/disable acquisition and (re)open the output file.
     *
     * @param enabled   Runtime gate. When false, records are silently dropped.
     * @param base_path Absolute path used as-is; a relative name goes to
     *                  ~/.qilab/logs/<name> (same directory as the profiler).
     *                  Empty uses the default "data_acquisition.jsonl".
     */
    void configure(bool enabled, const std::string &base_path = "");

    /// @brief True when acquisition is active (lock-free read).
    bool enabled() const noexcept;

    /**
     * @brief Append one complete JSONL line (thread-safe). Newline is added.
     *
     * Writes into the stream's in-memory buffer only; does NOT flush to the OS
     * on every call, so the hot path stays syscall-free. The stream still spills
     * to the OS when its buffer fills (see flush() to control when that happens).
     */
    void write_line(std::string_view json_line);

    /**
     * @brief Force buffered records to the OS (thread-safe).
     *
     * Call periodically from a NON real-time thread (or at shutdown) so the
     * write() syscall never lands on a latency-critical thread.
     */
    void flush();

private:
    DataAcquisition() = default;
    ~DataAcquisition();
    DataAcquisition(const DataAcquisition &) = delete;
    DataAcquisition(DataAcquisition &&) = delete;
    DataAcquisition &operator=(const DataAcquisition &) = delete;
    DataAcquisition &operator=(DataAcquisition &&) = delete;

    // Spill the in-memory accumulator to the stream once it grows past this, so
    // memory stays bounded. Caller must hold file_lock_.
    static constexpr std::size_t kSpillThresholdBytes = 1u << 20; // 1 MiB
    void spill_locked();

    std::atomic<bool> active_{false};
    std::mutex file_lock_;
    std::ofstream out_;
    std::string pending_; // in-memory accumulator; we control when it spills
};

/**
 * @brief Fluent JSON record builder.
 *
 * Accumulates key/value fields and commits a single JSONL line on destruction,
 * only if acquisition is enabled. Each record is prefixed with a "ts_us" epoch
 * microsecond timestamp and the "channel" name. String values are JSON-escaped.
 *
 * Low-latency: the record is built into a reused thread-local buffer using
 * std::to_chars (no per-record heap allocation after warm-up). A record must be
 * fully built and committed within a single statement (the fluent API enforces
 * this) — do not keep two live DataRecords on the same thread at once.
 *
 * @example
 *   DataRecord("entanglement").set("attempt", 42).set("fidelity", 0.91);
 */
class DataRecord
{
public:
    explicit DataRecord(std::string_view channel);
    ~DataRecord();

    DataRecord &set(std::string_view key, std::int64_t value);
    DataRecord &set(std::string_view key, double value);
    DataRecord &set(std::string_view key, bool value);
    DataRecord &set(std::string_view key, std::string_view value);

    // Convenience overloads so common integral literals bind unambiguously.
    DataRecord &set(std::string_view key, int value) { return set(key, static_cast<std::int64_t>(value)); }
    DataRecord &set(std::string_view key, std::size_t value) { return set(key, static_cast<std::int64_t>(value)); }
    DataRecord &set(std::string_view key, const char *value) { return set(key, std::string_view(value)); }

    DataRecord(const DataRecord &) = delete;
    DataRecord &operator=(const DataRecord &) = delete;

private:
    void append_key(std::string_view key);

    bool active_;        // snapshot of enabled() at construction
    std::string *buffer_; // reused thread-local accumulator (nullptr when inactive)
};

#endif // DAQ_ENABLED
