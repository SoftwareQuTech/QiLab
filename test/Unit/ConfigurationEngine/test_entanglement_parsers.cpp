// Unit tests for the entanglement-link model parsers, exercised directly
// against in-memory FieldMaps rather than through full YAML fixtures.
//
// These parsers are one layer below test_config_loader.cpp's YAML-fixture
// tests: they cover field-level validation of a single `attempt_model` /
// `channel_model` block, plus the type-name registry lookup that
// config_loader.cpp's parse_model_block() uses to select a parser.
//
// Parsers under test:
//   - config_parse::parse_entanglement_attempt_basic_model
//     (ConfigurationEngine/model_parsers/entanglement_attempt_basic_parser.hpp)
//   - config_parse::parse_entanglement_channel_basic_model
//     (ConfigurationEngine/model_parsers/entanglement_channel_basic_parser.hpp)
//   - config_parse::entanglement_attempt_model_parsers() /
//     config_parse::entanglement_channel_model_parsers()
//     (ConfigurationEngine/parse_entanglement_config.hpp)

#include <gtest/gtest.h>

#include <string>
#include <variant>

#include "ConfigurationEngine/model_parsers/entanglement_attempt_basic_parser.hpp"
#include "ConfigurationEngine/model_parsers/entanglement_channel_basic_parser.hpp"
#include "ConfigurationEngine/parse_entanglement_config.hpp"
#include "DataTypes/configuration_options.hpp"

namespace {

using config_parse::FieldMap;

// A valid field set for the attempt-model BASIC parser. Individual tests
// copy this and mutate/erase a single field.
FieldMap valid_attempt_fields()
{
    return FieldMap{
        {"type", "BASIC"},
        {"attempt_duration_ns", "1000"},
        {"batch_size", "100"},
        {"p_success", "0.5"},
        {"rng_seed", "42"},
    };
}

// A valid field set for the channel-model BASIC parser.
FieldMap valid_channel_fields()
{
    return FieldMap{
        {"type", "BASIC"},
        {"p_depolarizing", "0.01"},
        {"p_dephasing", "0.02"},
    };
}

// ---------------------------------------------------------------------------
// Attempt model: BASIC -- success path
// ---------------------------------------------------------------------------

TEST(EntanglementAttemptBasicParserTest, ParsesValidFieldsSuccessfully)
{
    const FieldMap fields = valid_attempt_fields();
    EntanglementAttemptConfigData output;
    std::string error;

    ASSERT_TRUE(config_parse::parse_entanglement_attempt_basic_model(fields, output, error));
    EXPECT_TRUE(error.empty());

    ASSERT_TRUE(std::holds_alternative<EntanglementAttemptBasicConfig>(output));
    const auto& parsed = std::get<EntanglementAttemptBasicConfig>(output);
    EXPECT_EQ(parsed.attempt_duration_ns, 1000u);
    EXPECT_EQ(parsed.batch_size, 100u);
    EXPECT_FLOAT_EQ(parsed.p_success, 0.5f);
    EXPECT_EQ(parsed.rng_seed, 42u);
}

TEST(EntanglementAttemptBasicParserTest, AcceptsPSuccessBoundaryValues)
{
    for (const std::string& p_success_text : {std::string("0"), std::string("1"), std::string("1.0")})
    {
        FieldMap fields = valid_attempt_fields();
        fields["p_success"] = p_success_text;

        EntanglementAttemptConfigData output;
        std::string error;
        ASSERT_TRUE(config_parse::parse_entanglement_attempt_basic_model(fields, output, error))
            << "p_success=" << p_success_text;
    }
}

TEST(EntanglementAttemptBasicParserTest, AcceptsBatchSizeAtUint32MaxBoundary)
{
    FieldMap fields = valid_attempt_fields();
    fields["batch_size"] = "4294967295"; // UINT32_MAX

    EntanglementAttemptConfigData output;
    std::string error;
    ASSERT_TRUE(config_parse::parse_entanglement_attempt_basic_model(fields, output, error));
    ASSERT_TRUE(std::holds_alternative<EntanglementAttemptBasicConfig>(output));
    EXPECT_EQ(std::get<EntanglementAttemptBasicConfig>(output).batch_size, 4294967295u);
}

// ---------------------------------------------------------------------------
// Attempt model: BASIC -- missing required keys
// ---------------------------------------------------------------------------

TEST(EntanglementAttemptBasicParserTest, RejectsMissingAttemptDurationNs)
{
    FieldMap fields = valid_attempt_fields();
    fields.erase("attempt_duration_ns");

    EntanglementAttemptConfigData output;
    std::string error;
    EXPECT_FALSE(config_parse::parse_entanglement_attempt_basic_model(fields, output, error));
    EXPECT_FALSE(error.empty());
}

TEST(EntanglementAttemptBasicParserTest, RejectsMissingBatchSize)
{
    FieldMap fields = valid_attempt_fields();
    fields.erase("batch_size");

    EntanglementAttemptConfigData output;
    std::string error;
    EXPECT_FALSE(config_parse::parse_entanglement_attempt_basic_model(fields, output, error));
    EXPECT_FALSE(error.empty());
}

TEST(EntanglementAttemptBasicParserTest, RejectsMissingPSuccess)
{
    FieldMap fields = valid_attempt_fields();
    fields.erase("p_success");

    EntanglementAttemptConfigData output;
    std::string error;
    EXPECT_FALSE(config_parse::parse_entanglement_attempt_basic_model(fields, output, error));
    EXPECT_FALSE(error.empty());
}

TEST(EntanglementAttemptBasicParserTest, RejectsMissingRngSeed)
{
    FieldMap fields = valid_attempt_fields();
    fields.erase("rng_seed");

    EntanglementAttemptConfigData output;
    std::string error;
    EXPECT_FALSE(config_parse::parse_entanglement_attempt_basic_model(fields, output, error));
    EXPECT_FALSE(error.empty());
}

// ---------------------------------------------------------------------------
// Attempt model: BASIC -- unknown key
// ---------------------------------------------------------------------------

TEST(EntanglementAttemptBasicParserTest, RejectsUnknownKey)
{
    FieldMap fields = valid_attempt_fields();
    fields["extra_field"] = "1";

    EntanglementAttemptConfigData output;
    std::string error;
    EXPECT_FALSE(config_parse::parse_entanglement_attempt_basic_model(fields, output, error));
    EXPECT_FALSE(error.empty());
}

// ---------------------------------------------------------------------------
// Attempt model: BASIC -- out-of-range / malformed numeric fields
// ---------------------------------------------------------------------------

TEST(EntanglementAttemptBasicParserTest, RejectsNegativeAttemptDurationNs)
{
    FieldMap fields = valid_attempt_fields();
    fields["attempt_duration_ns"] = "-1";

    EntanglementAttemptConfigData output;
    std::string error;
    EXPECT_FALSE(config_parse::parse_entanglement_attempt_basic_model(fields, output, error));
}

TEST(EntanglementAttemptBasicParserTest, RejectsNonNumericAttemptDurationNs)
{
    FieldMap fields = valid_attempt_fields();
    fields["attempt_duration_ns"] = "not_a_number";

    EntanglementAttemptConfigData output;
    std::string error;
    EXPECT_FALSE(config_parse::parse_entanglement_attempt_basic_model(fields, output, error));
}

TEST(EntanglementAttemptBasicParserTest, RejectsZeroBatchSize)
{
    FieldMap fields = valid_attempt_fields();
    fields["batch_size"] = "0";

    EntanglementAttemptConfigData output;
    std::string error;
    EXPECT_FALSE(config_parse::parse_entanglement_attempt_basic_model(fields, output, error));
}

TEST(EntanglementAttemptBasicParserTest, RejectsBatchSizeAboveUint32Max)
{
    FieldMap fields = valid_attempt_fields();
    fields["batch_size"] = "4294967296"; // UINT32_MAX + 1

    EntanglementAttemptConfigData output;
    std::string error;
    EXPECT_FALSE(config_parse::parse_entanglement_attempt_basic_model(fields, output, error));
}

TEST(EntanglementAttemptBasicParserTest, RejectsNegativeBatchSize)
{
    FieldMap fields = valid_attempt_fields();
    fields["batch_size"] = "-5";

    EntanglementAttemptConfigData output;
    std::string error;
    EXPECT_FALSE(config_parse::parse_entanglement_attempt_basic_model(fields, output, error));
}

TEST(EntanglementAttemptBasicParserTest, RejectsNonNumericBatchSize)
{
    FieldMap fields = valid_attempt_fields();
    fields["batch_size"] = "not_a_number";

    EntanglementAttemptConfigData output;
    std::string error;
    EXPECT_FALSE(config_parse::parse_entanglement_attempt_basic_model(fields, output, error));
}

TEST(EntanglementAttemptBasicParserTest, RejectsNegativePSuccess)
{
    FieldMap fields = valid_attempt_fields();
    fields["p_success"] = "-0.1";

    EntanglementAttemptConfigData output;
    std::string error;
    EXPECT_FALSE(config_parse::parse_entanglement_attempt_basic_model(fields, output, error));
}

TEST(EntanglementAttemptBasicParserTest, RejectsPSuccessAboveOne)
{
    FieldMap fields = valid_attempt_fields();
    fields["p_success"] = "1.5";

    EntanglementAttemptConfigData output;
    std::string error;
    EXPECT_FALSE(config_parse::parse_entanglement_attempt_basic_model(fields, output, error));
    EXPECT_NE(error.find("p_success"), std::string::npos);
}

TEST(EntanglementAttemptBasicParserTest, RejectsNonNumericPSuccess)
{
    FieldMap fields = valid_attempt_fields();
    fields["p_success"] = "not_a_number";

    EntanglementAttemptConfigData output;
    std::string error;
    EXPECT_FALSE(config_parse::parse_entanglement_attempt_basic_model(fields, output, error));
}

TEST(EntanglementAttemptBasicParserTest, RejectsNegativeRngSeed)
{
    FieldMap fields = valid_attempt_fields();
    fields["rng_seed"] = "-1";

    EntanglementAttemptConfigData output;
    std::string error;
    EXPECT_FALSE(config_parse::parse_entanglement_attempt_basic_model(fields, output, error));
}

TEST(EntanglementAttemptBasicParserTest, RejectsNonNumericRngSeed)
{
    FieldMap fields = valid_attempt_fields();
    fields["rng_seed"] = "not_a_number";

    EntanglementAttemptConfigData output;
    std::string error;
    EXPECT_FALSE(config_parse::parse_entanglement_attempt_basic_model(fields, output, error));
}

// ---------------------------------------------------------------------------
// Channel model: BASIC -- success path
// ---------------------------------------------------------------------------

TEST(EntanglementChannelBasicParserTest, ParsesValidFieldsSuccessfully)
{
    const FieldMap fields = valid_channel_fields();
    EntanglementChannelConfigData output;
    std::string error;

    ASSERT_TRUE(config_parse::parse_entanglement_channel_basic_model(fields, output, error));
    EXPECT_TRUE(error.empty());

    ASSERT_TRUE(std::holds_alternative<EntanglementChannelBasicConfig>(output));
    const auto& parsed = std::get<EntanglementChannelBasicConfig>(output);
    EXPECT_FLOAT_EQ(parsed.p_depolarizing, 0.01f);
    EXPECT_FLOAT_EQ(parsed.p_dephasing, 0.02f);
}

TEST(EntanglementChannelBasicParserTest, AcceptsBoundaryProbabilityValues)
{
    for (const std::string& value_text : {std::string("0"), std::string("1"), std::string("1.0")})
    {
        FieldMap fields = valid_channel_fields();
        fields["p_depolarizing"] = value_text;
        fields["p_dephasing"] = value_text;

        EntanglementChannelConfigData output;
        std::string error;
        ASSERT_TRUE(config_parse::parse_entanglement_channel_basic_model(fields, output, error))
            << "value=" << value_text;
    }
}

// ---------------------------------------------------------------------------
// Channel model: BASIC -- missing required keys
// ---------------------------------------------------------------------------

TEST(EntanglementChannelBasicParserTest, RejectsMissingPDepolarizing)
{
    FieldMap fields = valid_channel_fields();
    fields.erase("p_depolarizing");

    EntanglementChannelConfigData output;
    std::string error;
    EXPECT_FALSE(config_parse::parse_entanglement_channel_basic_model(fields, output, error));
    EXPECT_FALSE(error.empty());
}

TEST(EntanglementChannelBasicParserTest, RejectsMissingPDephasing)
{
    FieldMap fields = valid_channel_fields();
    fields.erase("p_dephasing");

    EntanglementChannelConfigData output;
    std::string error;
    EXPECT_FALSE(config_parse::parse_entanglement_channel_basic_model(fields, output, error));
    EXPECT_FALSE(error.empty());
}

// ---------------------------------------------------------------------------
// Channel model: BASIC -- unknown key
// ---------------------------------------------------------------------------

TEST(EntanglementChannelBasicParserTest, RejectsUnknownKey)
{
    FieldMap fields = valid_channel_fields();
    fields["extra_field"] = "1";

    EntanglementChannelConfigData output;
    std::string error;
    EXPECT_FALSE(config_parse::parse_entanglement_channel_basic_model(fields, output, error));
    EXPECT_FALSE(error.empty());
}

// ---------------------------------------------------------------------------
// Channel model: BASIC -- out-of-range / malformed numeric fields
// ---------------------------------------------------------------------------

TEST(EntanglementChannelBasicParserTest, RejectsNegativePDepolarizing)
{
    FieldMap fields = valid_channel_fields();
    fields["p_depolarizing"] = "-0.1";

    EntanglementChannelConfigData output;
    std::string error;
    EXPECT_FALSE(config_parse::parse_entanglement_channel_basic_model(fields, output, error));
}

TEST(EntanglementChannelBasicParserTest, RejectsPDepolarizingAboveOne)
{
    FieldMap fields = valid_channel_fields();
    fields["p_depolarizing"] = "1.5";

    EntanglementChannelConfigData output;
    std::string error;
    EXPECT_FALSE(config_parse::parse_entanglement_channel_basic_model(fields, output, error));
    EXPECT_NE(error.find("p_depolarizing"), std::string::npos);
}

TEST(EntanglementChannelBasicParserTest, RejectsNonNumericPDepolarizing)
{
    FieldMap fields = valid_channel_fields();
    fields["p_depolarizing"] = "not_a_number";

    EntanglementChannelConfigData output;
    std::string error;
    EXPECT_FALSE(config_parse::parse_entanglement_channel_basic_model(fields, output, error));
}

TEST(EntanglementChannelBasicParserTest, RejectsNegativePDephasing)
{
    FieldMap fields = valid_channel_fields();
    fields["p_dephasing"] = "-0.1";

    EntanglementChannelConfigData output;
    std::string error;
    EXPECT_FALSE(config_parse::parse_entanglement_channel_basic_model(fields, output, error));
}

TEST(EntanglementChannelBasicParserTest, RejectsPDephasingAboveOne)
{
    FieldMap fields = valid_channel_fields();
    fields["p_dephasing"] = "1.5";

    EntanglementChannelConfigData output;
    std::string error;
    EXPECT_FALSE(config_parse::parse_entanglement_channel_basic_model(fields, output, error));
    EXPECT_NE(error.find("p_dephasing"), std::string::npos);
}

TEST(EntanglementChannelBasicParserTest, RejectsNonNumericPDephasing)
{
    FieldMap fields = valid_channel_fields();
    fields["p_dephasing"] = "not_a_number";

    EntanglementChannelConfigData output;
    std::string error;
    EXPECT_FALSE(config_parse::parse_entanglement_channel_basic_model(fields, output, error));
}

// ---------------------------------------------------------------------------
// Type-name registry lookup (config_parse::entanglement_attempt_model_parsers()
// / entanglement_channel_model_parsers()).
//
// The actual dispatch call site is config_loader.cpp's parse_model_block(),
// a private template in that .cpp (not exposed via a header), so it can't be
// invoked directly at this layer. What parse_entanglement_config.hpp itself
// owns -- and what's tested here -- is the registry content: that "BASIC" is
// registered and resolves to a working parser, and that an unrecognized type
// name is simply absent from the map (the same failure mode parse_model_block
// checks for via parser_map.find(...) == parser_map.end()).
// ---------------------------------------------------------------------------

TEST(EntanglementModelRegistryTest, AttemptRegistryContainsWorkingBasicParser)
{
    const auto& parsers = config_parse::entanglement_attempt_model_parsers();
    const auto parser_it = parsers.find("BASIC");
    ASSERT_NE(parser_it, parsers.end());

    const FieldMap fields = valid_attempt_fields();
    EntanglementAttemptConfigData output;
    std::string error;
    EXPECT_TRUE(parser_it->second(fields, output, error));
}

TEST(EntanglementModelRegistryTest, AttemptRegistryRejectsUnknownType)
{
    const auto& parsers = config_parse::entanglement_attempt_model_parsers();
    EXPECT_EQ(parsers.find("UNKNOWN_TYPE"), parsers.end());
}

TEST(EntanglementModelRegistryTest, ChannelRegistryContainsWorkingBasicParser)
{
    const auto& parsers = config_parse::entanglement_channel_model_parsers();
    const auto parser_it = parsers.find("BASIC");
    ASSERT_NE(parser_it, parsers.end());

    const FieldMap fields = valid_channel_fields();
    EntanglementChannelConfigData output;
    std::string error;
    EXPECT_TRUE(parser_it->second(fields, output, error));
}

TEST(EntanglementModelRegistryTest, ChannelRegistryRejectsUnknownType)
{
    const auto& parsers = config_parse::entanglement_channel_model_parsers();
    EXPECT_EQ(parsers.find("UNKNOWN_TYPE"), parsers.end());
}

} // namespace
