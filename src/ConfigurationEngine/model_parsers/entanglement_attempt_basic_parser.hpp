#pragma once

#include <cstdint>
#include <string>

#include "ConfigurationEngine/parse_scalar_helpers.hpp"
#include "DataTypes/configuration_options.hpp"

namespace config_parse {

inline bool parse_entanglement_attempt_basic_model(const FieldMap& fields,
                                                    EntanglementAttemptConfigData& output,
                                                    std::string& error) noexcept
{
    if (!ensure_allowed_keys(fields, {"type", "attempt_duration_ns", "batch_size", "p_success", "rng_seed"}, error))
    {
        return false;
    }

    std::string attempt_duration_text;
    if (!require_key(fields, "attempt_duration_ns", attempt_duration_text, error))
    {
        return false;
    }

    uint64_t attempt_duration_ns = 0;
    if (!parse_non_negative_u64(attempt_duration_text, attempt_duration_ns))
    {
        error = "Invalid value for 'attempt_duration_ns': " + attempt_duration_text;
        return false;
    }

    std::string batch_size_text;
    if (!require_key(fields, "batch_size", batch_size_text, error))
    {
        return false;
    }

    uint64_t batch_size_value = 0;
    if (!parse_non_negative_u64(batch_size_text, batch_size_value) ||
        batch_size_value == 0 || batch_size_value > static_cast<uint64_t>(UINT32_MAX))
    {
        error = "Invalid value for 'batch_size': " + batch_size_text;
        return false;
    }

    std::string p_success_text;
    if (!require_key(fields, "p_success", p_success_text, error))
    {
        return false;
    }

    float p_success = 0.0f;
    if (!parse_non_negative_float(p_success_text, p_success) || p_success > 1.0f)
    {
        error = "Invalid value for 'p_success': " + p_success_text + " (must be in [0, 1])";
        return false;
    }

    std::string rng_seed_text;
    if (!require_key(fields, "rng_seed", rng_seed_text, error))
    {
        return false;
    }

    uint64_t rng_seed = 0;
    if (!parse_non_negative_u64(rng_seed_text, rng_seed))
    {
        error = "Invalid value for 'rng_seed': " + rng_seed_text;
        return false;
    }

    output = EntanglementAttemptBasicConfig{
        .attempt_duration_ns = attempt_duration_ns,
        .batch_size = static_cast<uint32_t>(batch_size_value),
        .p_success = p_success,
        .rng_seed = rng_seed};
    return true;
}

} // namespace config_parse
