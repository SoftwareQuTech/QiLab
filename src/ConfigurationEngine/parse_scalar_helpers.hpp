#pragma once

#include <cctype>
#include <cstdint>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <initializer_list>
#include <optional>
#include <string>
#include <system_error>
#include <unordered_map>

namespace config_parse {

using FieldMap = std::unordered_map<std::string, std::string>;

inline std::string trim(const std::string& input) noexcept
{
    std::size_t begin = 0;
    while (begin < input.size() && std::isspace(static_cast<unsigned char>(input[begin])) != 0)
    {
        ++begin;
    }

    std::size_t end = input.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(input[end - 1])) != 0)
    {
        --end;
    }

    return input.substr(begin, end - begin);
}

inline std::optional<std::pair<std::string, std::string>> split_key_value(const std::string& content) noexcept
{
    const std::size_t separator = content.find(':');
    if (separator == std::string::npos)
    {
        return std::nullopt;
    }

    const std::string key = trim(content.substr(0, separator));
    const std::string value = trim(content.substr(separator + 1));
    if (key.empty())
    {
        return std::nullopt;
    }

    return std::make_pair(key, value);
}

inline bool parse_positive_int(const std::string& text, int& value) noexcept
{
    const std::string normalized = trim(text);
    if (normalized.empty())
    {
        return false;
    }

    int parsed_value = 0;
    const char* begin = normalized.data();
    const char* end = normalized.data() + normalized.size();
    const auto [ptr, ec] = std::from_chars(begin, end, parsed_value);
    if (ec != std::errc() || ptr != end || parsed_value <= 0)
    {
        return false;
    }

    value = parsed_value;
    return true;
}

inline bool parse_non_negative_float(const std::string& text, float& value) noexcept
{
    const std::string normalized = trim(text);
    if (normalized.empty())
    {
        return false;
    }

    char* parse_end = nullptr;
    const float parsed_value = std::strtof(normalized.c_str(), &parse_end);
    if (parse_end == nullptr || *parse_end != '\0' || !std::isfinite(parsed_value) || parsed_value < 0.0f)
    {
        return false;
    }

    value = parsed_value;
    return true;
}

inline bool parse_non_negative_u16(const std::string& text, uint16_t& value) noexcept
{
    const std::string normalized = trim(text);
    if (normalized.empty())
    {
        return false;
    }

    unsigned int parsed_value = 0;
    const char* begin = normalized.data();
    const char* end = normalized.data() + normalized.size();
    const auto [ptr, ec] = std::from_chars(begin, end, parsed_value);
    if (ec != std::errc() || ptr != end || parsed_value > static_cast<unsigned int>(UINT16_MAX))
    {
        return false;
    }

    value = static_cast<uint16_t>(parsed_value);
    return true;
}
inline bool parse_non_negative_u64(const std::string& text, uint64_t& value) noexcept
{
    const std::string normalized = trim(text);
    if (normalized.empty())
    {
        return false;
    }

    unsigned long long parsed_value = 0;
    const char* begin = normalized.data();
    const char* end = normalized.data() + normalized.size();
    const auto [ptr, ec] = std::from_chars(begin, end, parsed_value);
    if (ec != std::errc() || ptr != end || parsed_value > static_cast<unsigned long long>(UINT64_MAX))
    {
        return false;
    }

    value = static_cast<uint64_t>(parsed_value);
    return true;
}

inline bool require_key(const FieldMap& fields,
                        const std::string& key,
                        std::string& value,
                        std::string& error) noexcept
{
    const auto it = fields.find(key);
    if (it == fields.end())
    {
        error = "Missing required key '" + key + "'";
        return false;
    }

    value = it->second;
    return true;
}

inline bool ensure_allowed_keys(const FieldMap& fields,
                                std::initializer_list<const char*> allowed,
                                std::string& error) noexcept
{
    for (const auto& kv : fields)
    {
        bool found = false;
        for (const char* key : allowed)
        {
            if (kv.first == key)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            error = "Unknown key '" + kv.first + "'";
            return false;
        }
    }

    return true;
}

} // namespace config_parse
