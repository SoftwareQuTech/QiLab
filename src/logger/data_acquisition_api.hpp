// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

/**
 * @file data_acquisition_api.hpp
 * @brief Public API for structured research data acquisition.
 *
 * Provides a macro/builder API for recording arbitrary structured data to a
 * dedicated JSONL file, separate from the diagnostic StandardLogger. Zero
 * overhead and no linking when DAQ_ENABLED == 0.
 *
 * ## Usage Example
 * @code
 *   // Once at startup (e.g. from an env var):
 *   DAQ_CONFIGURE(true, "");   // "" -> default path ~/.qilab/logs/data_acquisition.jsonl
 *
 *   // Anywhere, from any thread:
 *   DAQ_RECORD("entanglement").set("attempt", 42).set("success", true).set("fidelity", 0.91);
 *
 *   // Optionally guard expensive field computation:
 *   if (DAQ_ACTIVE()) {
 *       DAQ_RECORD("state").set("dump", expensive_to_string());
 *   }
 * @endcode
 *
 * Output is one JSON object per line, e.g.:
 *   {"ts_us":1751371200123456,"channel":"entanglement","attempt":42,"success":true,"fidelity":0.910000}
 * Load for analysis with: pandas.read_json(path, lines=True)
 */

#pragma once

#if DAQ_ENABLED

#include "data_acquisition.hpp"

/**
 * @brief Build and commit one structured record for @p channel.
 *
 * Returns a DataRecord temporary; chain .set(key, value) calls. The record is
 * written on destruction (end of the full expression), only if acquisition is
 * enabled. Thread-safe.
 */
#define DAQ_RECORD(channel) DataRecord(channel)

/**
 * @brief Enable/disable acquisition and open the output file.
 * @param enabled Runtime gate (bool).
 * @param path    Output path/filename; "" -> ~/.qilab/logs/data_acquisition.jsonl.
 */
#define DAQ_CONFIGURE(enabled, path) ::DataAcquisition::instance().configure((enabled), (path))

/// @brief True when acquisition is active; use to skip expensive field building.
#define DAQ_ACTIVE() ::DataAcquisition::instance().enabled()

/**
 * @brief Force buffered records to disk. Call from a NON real-time thread (or at
 * shutdown) so the write() syscall never lands on a latency-critical thread.
 */
#define DAQ_FLUSH() ::DataAcquisition::instance().flush()

#else // DAQ_ENABLED == 0  ========== DATA ACQUISITION DISABLED ==========

#include <cstddef>
#include <string_view>

/**
 * @brief Zero-overhead stub. Accepts and discards any chained set() calls so
 * instrumented code compiles and links without the implementation.
 */
struct DataRecord
{
    explicit DataRecord(std::string_view) {}
    template <typename K, typename V>
    DataRecord &set(const K &, const V &) { return *this; }
};

#define DAQ_RECORD(channel) DataRecord(channel)

#define DAQ_CONFIGURE(enabled, path) \
    do                               \
    {                                \
    } while (0)

#define DAQ_ACTIVE() (false)

#define DAQ_FLUSH() \
    do              \
    {               \
    } while (0)

#endif // DAQ_ENABLED
