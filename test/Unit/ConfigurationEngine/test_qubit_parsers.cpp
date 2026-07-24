#include <gtest/gtest.h>

#include <string>
#include <variant>

#include "ConfigurationEngine/model_parsers/qubit_advanced_parser.hpp"
#include "ConfigurationEngine/model_parsers/qubit_basic_parser.hpp"
#include "ConfigurationEngine/model_parsers/qubit_phase_flip_parser.hpp"
#include "ConfigurationEngine/parse_scalar_helpers.hpp"
#include "DataTypes/configuration_options.hpp"

// Direct, isolated unit tests for the individual qubit noise model parsers
// (BASIC, ADVANCED, PHASE_FLIP). These call parse_<model>_model() directly
// with in-memory FieldMap instances, rather than going through a full YAML
// fixture round-trip (see test_config_loader.cpp for that level of test).

namespace {

using config_parse::FieldMap;

// ---------------------------------------------------------------------------
// BASIC
// ---------------------------------------------------------------------------

TEST(QubitBasicParserTest, ParsesValidFields)
{
    const FieldMap fields = {{"type", "BASIC"}, {"t2_value", "1234.5"}};
    QubitNoiseConfigData output;
    std::string error;

    ASSERT_TRUE(config_parse::parse_qubit_basic_model(fields, output, error));
    EXPECT_TRUE(error.empty());
    ASSERT_TRUE(std::holds_alternative<QubitNoiseBasicConfig>(output));
    EXPECT_FLOAT_EQ(std::get<QubitNoiseBasicConfig>(output).t2_value, 1234.5f);
}

TEST(QubitBasicParserTest, RejectsMissingT2Value)
{
    const FieldMap fields = {{"type", "BASIC"}};
    QubitNoiseConfigData output;
    std::string error;

    EXPECT_FALSE(config_parse::parse_qubit_basic_model(fields, output, error));
    EXPECT_FALSE(error.empty());
}

TEST(QubitBasicParserTest, RejectsUnknownKey)
{
    const FieldMap fields = {{"type", "BASIC"}, {"t2_value", "1000"}, {"extra_field", "1"}};
    QubitNoiseConfigData output;
    std::string error;

    EXPECT_FALSE(config_parse::parse_qubit_basic_model(fields, output, error));
    EXPECT_FALSE(error.empty());
}

TEST(QubitBasicParserTest, RejectsNonNumericT2Value)
{
    const FieldMap fields = {{"type", "BASIC"}, {"t2_value", "not_a_number"}};
    QubitNoiseConfigData output;
    std::string error;

    EXPECT_FALSE(config_parse::parse_qubit_basic_model(fields, output, error));
    EXPECT_FALSE(error.empty());
}

TEST(QubitBasicParserTest, RejectsNegativeT2Value)
{
    const FieldMap fields = {{"type", "BASIC"}, {"t2_value", "-100"}};
    QubitNoiseConfigData output;
    std::string error;

    EXPECT_FALSE(config_parse::parse_qubit_basic_model(fields, output, error));
    EXPECT_FALSE(error.empty());
}

TEST(QubitBasicParserTest, AcceptsZeroT2Value)
{
    const FieldMap fields = {{"type", "BASIC"}, {"t2_value", "0"}};
    QubitNoiseConfigData output;
    std::string error;

    ASSERT_TRUE(config_parse::parse_qubit_basic_model(fields, output, error));
    ASSERT_TRUE(std::holds_alternative<QubitNoiseBasicConfig>(output));
    EXPECT_FLOAT_EQ(std::get<QubitNoiseBasicConfig>(output).t2_value, 0.0f);
}

TEST(QubitBasicParserTest, RejectsTrailingGarbageInNumericValue)
{
    // parse_non_negative_float requires the whole (trimmed) string to be consumed.
    const FieldMap fields = {{"type", "BASIC"}, {"t2_value", "100abc"}};
    QubitNoiseConfigData output;
    std::string error;

    EXPECT_FALSE(config_parse::parse_qubit_basic_model(fields, output, error));
}

// ---------------------------------------------------------------------------
// ADVANCED
// ---------------------------------------------------------------------------

TEST(QubitAdvancedParserTest, ParsesValidFields)
{
    const FieldMap fields = {
        {"type", "ADVANCED"},
        {"t1_value", "6000"},
        {"t2_value", "3000"},
        {"gate_error_rate", "0.002"},
    };
    QubitNoiseConfigData output;
    std::string error;

    ASSERT_TRUE(config_parse::parse_qubit_advanced_model(fields, output, error));
    EXPECT_TRUE(error.empty());
    ASSERT_TRUE(std::holds_alternative<QubitNoiseAdvancedConfig>(output));
    const auto& advanced = std::get<QubitNoiseAdvancedConfig>(output);
    EXPECT_FLOAT_EQ(advanced.t1_value, 6000.0f);
    EXPECT_FLOAT_EQ(advanced.t2_value, 3000.0f);
    EXPECT_FLOAT_EQ(advanced.gate_error_rate, 0.002f);
}

TEST(QubitAdvancedParserTest, RejectsMissingT1Value)
{
    const FieldMap fields = {
        {"type", "ADVANCED"},
        {"t2_value", "3000"},
        {"gate_error_rate", "0.001"},
    };
    QubitNoiseConfigData output;
    std::string error;

    EXPECT_FALSE(config_parse::parse_qubit_advanced_model(fields, output, error));
    EXPECT_FALSE(error.empty());
}

TEST(QubitAdvancedParserTest, RejectsMissingT2Value)
{
    const FieldMap fields = {
        {"type", "ADVANCED"},
        {"t1_value", "6000"},
        {"gate_error_rate", "0.001"},
    };
    QubitNoiseConfigData output;
    std::string error;

    EXPECT_FALSE(config_parse::parse_qubit_advanced_model(fields, output, error));
    EXPECT_FALSE(error.empty());
}

TEST(QubitAdvancedParserTest, RejectsMissingGateErrorRate)
{
    const FieldMap fields = {
        {"type", "ADVANCED"},
        {"t1_value", "6000"},
        {"t2_value", "3000"},
    };
    QubitNoiseConfigData output;
    std::string error;

    EXPECT_FALSE(config_parse::parse_qubit_advanced_model(fields, output, error));
    EXPECT_FALSE(error.empty());
}

TEST(QubitAdvancedParserTest, RejectsUnknownKey)
{
    const FieldMap fields = {
        {"type", "ADVANCED"},
        {"t1_value", "6000"},
        {"t2_value", "3000"},
        {"gate_error_rate", "0.001"},
        {"unexpected", "1"},
    };
    QubitNoiseConfigData output;
    std::string error;

    EXPECT_FALSE(config_parse::parse_qubit_advanced_model(fields, output, error));
    EXPECT_FALSE(error.empty());
}

TEST(QubitAdvancedParserTest, RejectsNegativeGateErrorRate)
{
    const FieldMap fields = {
        {"type", "ADVANCED"},
        {"t1_value", "6000"},
        {"t2_value", "3000"},
        {"gate_error_rate", "-0.001"},
    };
    QubitNoiseConfigData output;
    std::string error;

    EXPECT_FALSE(config_parse::parse_qubit_advanced_model(fields, output, error));
    EXPECT_FALSE(error.empty());
}

TEST(QubitAdvancedParserTest, RejectsNonNumericT1Value)
{
    const FieldMap fields = {
        {"type", "ADVANCED"},
        {"t1_value", "abc"},
        {"t2_value", "3000"},
        {"gate_error_rate", "0.001"},
    };
    QubitNoiseConfigData output;
    std::string error;

    EXPECT_FALSE(config_parse::parse_qubit_advanced_model(fields, output, error));
    EXPECT_FALSE(error.empty());
}

TEST(QubitAdvancedParserTest, AcceptsAllZeroValues)
{
    const FieldMap fields = {
        {"type", "ADVANCED"},
        {"t1_value", "0"},
        {"t2_value", "0"},
        {"gate_error_rate", "0"},
    };
    QubitNoiseConfigData output;
    std::string error;

    ASSERT_TRUE(config_parse::parse_qubit_advanced_model(fields, output, error));
    ASSERT_TRUE(std::holds_alternative<QubitNoiseAdvancedConfig>(output));
    const auto& advanced = std::get<QubitNoiseAdvancedConfig>(output);
    EXPECT_FLOAT_EQ(advanced.t1_value, 0.0f);
    EXPECT_FLOAT_EQ(advanced.t2_value, 0.0f);
    EXPECT_FLOAT_EQ(advanced.gate_error_rate, 0.0f);
}

// ---------------------------------------------------------------------------
// PHASE_FLIP
// ---------------------------------------------------------------------------

TEST(QubitPhaseFlipParserTest, ParsesValidFields)
{
    const FieldMap fields = {{"type", "PHASE_FLIP"}, {"t2_value", "100"}};
    QubitNoiseConfigData output;
    std::string error;

    ASSERT_TRUE(config_parse::parse_qubit_phase_flip_model(fields, output, error));
    EXPECT_TRUE(error.empty());
    ASSERT_TRUE(std::holds_alternative<PhaseFlipNoiseConfig>(output));
    EXPECT_FLOAT_EQ(std::get<PhaseFlipNoiseConfig>(output).t2_value, 100.0f);
}

TEST(QubitPhaseFlipParserTest, RejectsMissingT2Value)
{
    const FieldMap fields = {{"type", "PHASE_FLIP"}};
    QubitNoiseConfigData output;
    std::string error;

    EXPECT_FALSE(config_parse::parse_qubit_phase_flip_model(fields, output, error));
    EXPECT_FALSE(error.empty());
}

TEST(QubitPhaseFlipParserTest, RejectsUnknownKey)
{
    const FieldMap fields = {{"type", "PHASE_FLIP"}, {"t2_value", "100"}, {"bogus", "x"}};
    QubitNoiseConfigData output;
    std::string error;

    EXPECT_FALSE(config_parse::parse_qubit_phase_flip_model(fields, output, error));
    EXPECT_FALSE(error.empty());
}

TEST(QubitPhaseFlipParserTest, RejectsNegativeT2Value)
{
    const FieldMap fields = {{"type", "PHASE_FLIP"}, {"t2_value", "-1"}};
    QubitNoiseConfigData output;
    std::string error;

    EXPECT_FALSE(config_parse::parse_qubit_phase_flip_model(fields, output, error));
    EXPECT_FALSE(error.empty());
}

TEST(QubitPhaseFlipParserTest, RejectsNonNumericT2Value)
{
    const FieldMap fields = {{"type", "PHASE_FLIP"}, {"t2_value", "NaN_text"}};
    QubitNoiseConfigData output;
    std::string error;

    EXPECT_FALSE(config_parse::parse_qubit_phase_flip_model(fields, output, error));
    EXPECT_FALSE(error.empty());
}

} // namespace
