#include "data_acquisition.hpp"
#if DAQ_ENABLED

#include <chrono>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <cstdlib>

#include "logger/logger_api.hpp"

namespace {

// Reused per-thread scratch buffer for building a record. Reusing it means no
// heap allocation per record after warm-up (only clear(), which keeps capacity).
thread_local std::string tls_record_buffer;

// Append an integer/double as a JSON-numeric token without allocating.
void append_number(std::string &out, std::int64_t value)
{
    char buf[24];
    auto r = std::to_chars(buf, buf + sizeof(buf), value);
    out.append(buf, r.ptr);
}

void append_number(std::string &out, double value)
{
    if (!std::isfinite(value)) {
        out += "null"; // JSON has no NaN/Inf
        return;
    }
    char buf[32];
    auto r = std::to_chars(buf, buf + sizeof(buf), value);
    out.append(buf, r.ptr);
}

// Absolute paths (e.g. from tests) are honored as-is; anything else lands in
// ~/.qilab/logs/ next to the profiler output, so all instrument data lives in
// one place.
std::string resolve_daq_path(const std::string &base_path)
{
    namespace fs = std::filesystem;

    fs::path base = base_path.empty() ? fs::path("data_acquisition.jsonl") : fs::path(base_path);
    if (base.is_absolute()) {
        return base.string();
    }

    const char *home = std::getenv("HOME");
    const fs::path dir = home ? fs::path(home) / ".qilab" / "logs" : fs::path(".qilab") / "logs";
    return (dir / base).string();
}

// Append `value` to `out` as a JSON string literal, escaping per RFC 8259.
void append_json_string(std::string &out, std::string_view value)
{
    out.push_back('"');
    for (char c : value) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[7];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    out += buf;
                } else {
                    out.push_back(c);
                }
        }
    }
    out.push_back('"');
}

std::int64_t now_epoch_us()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

} // namespace

// ----------------- DataAcquisition -----------------

DataAcquisition &DataAcquisition::instance()
{
    static DataAcquisition inst;
    return inst;
}

void DataAcquisition::configure(bool enabled, const std::string &base_path)
{
    std::lock_guard<std::mutex> lock(file_lock_);
    active_.store(false, std::memory_order_relaxed);

    if (out_.is_open()) {
        spill_locked(); // don't lose records buffered under the previous config
        out_.flush();
        out_.close();
    }

    if (!enabled) {
        return;
    }

    const std::string resolved = resolve_daq_path(base_path);
    const std::filesystem::path resolved_path{resolved};
    const auto parent_dir = resolved_path.parent_path();
    if (!parent_dir.empty()) {
        std::filesystem::create_directories(parent_dir);
    }

    out_.open(resolved, std::ios::out | std::ios::app);
    if (!out_.is_open()) {
        LOG_ERROR("DataAcquisition: failed to open output file: " + resolved);
        return;
    }

    // Pre-size the accumulator so steady-state appends never reallocate.
    pending_.clear();
    pending_.reserve(kSpillThresholdBytes + 4096);

    active_.store(true, std::memory_order_relaxed);
    LOG_INFO("DataAcquisition enabled, writing to: " + resolved);
}

DataAcquisition::~DataAcquisition()
{
    // Runs at program exit: hand any buffered records to the OS before members die.
    std::lock_guard<std::mutex> lock(file_lock_);
    if (out_.is_open()) {
        spill_locked();
        out_.flush();
        out_.close();
    }
}

// Caller must hold file_lock_. Moves the accumulator into the stream. This is
// where the write() syscall(s) happen, so it only runs on flush()/spill/close,
// never per record.
void DataAcquisition::spill_locked()
{
    if (pending_.empty() || !out_.is_open()) {
        return;
    }
    out_.write(pending_.data(), static_cast<std::streamsize>(pending_.size()));
    pending_.clear();
}

bool DataAcquisition::enabled() const noexcept
{
    return active_.load(std::memory_order_relaxed);
}

void DataAcquisition::write_line(std::string_view json_line)
{
    if (!active_.load(std::memory_order_relaxed)) {
        return;
    }
    std::lock_guard<std::mutex> lock(file_lock_);
    // Hot path: just a memcpy into our own in-memory accumulator. No syscall.
    pending_.append(json_line.data(), json_line.size());
    pending_.push_back('\n');
    // Bound memory: spill only when we cross the threshold. The rare thread that
    // trips this pays the write() syscall; call flush() from a non-RT thread to
    // keep that off the hot path.
    if (pending_.size() >= kSpillThresholdBytes) {
        spill_locked();
    }
}

void DataAcquisition::flush()
{
    std::lock_guard<std::mutex> lock(file_lock_);
    spill_locked();
    if (out_.is_open()) {
        out_.flush();
    }
}

// ----------------- DataRecord -----------------

DataRecord::DataRecord(std::string_view channel)
    : active_(DataAcquisition::instance().enabled()), buffer_(nullptr)
{
    if (!active_) {
        return;
    }
    buffer_ = &tls_record_buffer;
    buffer_->clear(); // keeps capacity: no allocation after warm-up
    *buffer_ += "{\"ts_us\":";
    append_number(*buffer_, now_epoch_us());
    *buffer_ += ",\"channel\":";
    append_json_string(*buffer_, channel);
}

DataRecord::~DataRecord()
{
    if (!active_) {
        return;
    }
    buffer_->push_back('}');
    DataAcquisition::instance().write_line(*buffer_);
}

void DataRecord::append_key(std::string_view key)
{
    buffer_->push_back(',');
    append_json_string(*buffer_, key);
    buffer_->push_back(':');
}

DataRecord &DataRecord::set(std::string_view key, std::int64_t value)
{
    if (active_) {
        append_key(key);
        append_number(*buffer_, value);
    }
    return *this;
}

DataRecord &DataRecord::set(std::string_view key, double value)
{
    if (active_) {
        append_key(key);
        append_number(*buffer_, value); // handles NaN/Inf -> null
    }
    return *this;
}

DataRecord &DataRecord::set(std::string_view key, bool value)
{
    if (active_) {
        append_key(key);
        *buffer_ += value ? "true" : "false";
    }
    return *this;
}

DataRecord &DataRecord::set(std::string_view key, std::string_view value)
{
    if (active_) {
        append_key(key);
        append_json_string(*buffer_, value);
    }
    return *this;
}

#endif // DAQ_ENABLED
