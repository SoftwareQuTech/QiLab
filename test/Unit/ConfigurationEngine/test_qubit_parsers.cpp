// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <gtest/gtest.h>

#include <initializer_list>
#include <string>
#include <utility>
#include <variant>

#include "ConfigurationEngine/model_parsers/gate_noise_depolarizing_parser.hpp"
#include "ConfigurationEngine/model_parsers/qubit_advanced_parser.hpp"
#include "ConfigurationEngine/model_parsers/qubit_basic_parser.hpp"
#include "ConfigurationEngine/model_parsers/qubit_phase_flip_parser.hpp"
#include "ConfigurationEngine/parse_scalar_helpers.hpp"
#include "DataTypes/configuration_options.hpp"

// Direct, isolated unit tests for the individual qubit noise model parsers
// (BASIC, ADVANCED, PHASE_FLIP) and the per-qubit-array gate noise parser
// (DEPOLARIZING). These call parse_<model>_model() directly
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

// ---------------------------------------------------------------------------
// Gate noise: DEPOLARIZING (per qubit array, Sec. 2 Eq. 1)
// ---------------------------------------------------------------------------

TEST(GateNoiseDepolarizingParserTest, ParsesValidFields)
{
    const FieldMap fields = {{"type", "DEPOLARIZING"}, {"depolarizing_probability", "0.01"}};
    GateNoiseConfigData output;
    std::string error;

    ASSERT_TRUE(config_parse::parse_single_qubit_gate_noise_depolarizing_model(fields, output, error)) << error;
    ASSERT_TRUE(std::holds_alternative<GateNoiseDepolarizingConfig>(output));
    EXPECT_FLOAT_EQ(std::get<GateNoiseDepolarizingConfig>(output).depolarizing_probability, 0.01f);
}

TEST(GateNoiseDepolarizingParserTest, AcceptsBoundaryProbabilities)
{
    for (const char* text : {"0", "0.75"})
    {
        const FieldMap fields = {{"type", "DEPOLARIZING"}, {"depolarizing_probability", text}};
        GateNoiseConfigData output;
        std::string error;

        EXPECT_TRUE(config_parse::parse_single_qubit_gate_noise_depolarizing_model(fields, output, error))
            << text << ": " << error;
    }
}

TEST(GateNoiseDepolarizingParserTest, RejectsMissingProbability)
{
    const FieldMap fields = {{"type", "DEPOLARIZING"}};
    GateNoiseConfigData output;
    std::string error;

    EXPECT_FALSE(config_parse::parse_single_qubit_gate_noise_depolarizing_model(fields, output, error));
    EXPECT_FALSE(error.empty());
}

TEST(GateNoiseDepolarizingParserTest, RejectsUnknownKey)
{
    const FieldMap fields = {
        {"type", "DEPOLARIZING"}, {"depolarizing_probability", "0.01"}, {"bogus", "x"}};
    GateNoiseConfigData output;
    std::string error;

    EXPECT_FALSE(config_parse::parse_single_qubit_gate_noise_depolarizing_model(fields, output, error));
    EXPECT_FALSE(error.empty());
}

// The bound is the channel's maximal mixing point, taken from the model class,
// so an unusable probability is refused at load time rather than accepted and
// then silently downgraded to noiseless at registration.
TEST(GateNoiseDepolarizingParserTest, RejectsProbabilityAboveSingleQubitMaximalMixing)
{
    for (const char* text : {"-0.1", "0.76", "0.9", "1.5"})
    {
        const FieldMap fields = {{"type", "DEPOLARIZING"}, {"depolarizing_probability", text}};
        GateNoiseConfigData output;
        std::string error;

        EXPECT_FALSE(config_parse::parse_single_qubit_gate_noise_depolarizing_model(fields, output, error)) << text;
        EXPECT_FALSE(error.empty());
    }
}

TEST(GateNoiseDepolarizingParserTest, RejectsNonNumericProbability)
{
    const FieldMap fields = {{"type", "DEPOLARIZING"}, {"depolarizing_probability", "low"}};
    GateNoiseConfigData output;
    std::string error;

    EXPECT_FALSE(config_parse::parse_single_qubit_gate_noise_depolarizing_model(fields, output, error));
    EXPECT_FALSE(error.empty());
}

// The two-qubit channel mixes maximally at a higher p, so its parser accepts a
// range the single-qubit one refuses. Each parser's bound comes from its own
// model class, so the two cannot drift apart.
TEST(GateNoiseDepolarizingParserTest, TwoQubitParserAcceptsItsOwnWiderRange)
{
    const FieldMap fields = {{"type", "DEPOLARIZING"}, {"depolarizing_probability", "0.9"}};
    std::string error;

    GateNoiseConfigData two_qubit_output;
    ASSERT_TRUE(config_parse::parse_two_qubit_gate_noise_depolarizing_model(fields, two_qubit_output, error))
        << error;
    EXPECT_FLOAT_EQ(std::get<GateNoiseDepolarizingConfig>(two_qubit_output).depolarizing_probability, 0.9f);

    GateNoiseConfigData single_qubit_output;
    EXPECT_FALSE(
        config_parse::parse_single_qubit_gate_noise_depolarizing_model(fields, single_qubit_output, error));
}

TEST(GateNoiseDepolarizingParserTest, RejectsProbabilityAboveTwoQubitMaximalMixing)
{
    const FieldMap fields = {{"type", "DEPOLARIZING"}, {"depolarizing_probability", "0.95"}};
    GateNoiseConfigData output;
    std::string error;

    EXPECT_FALSE(config_parse::parse_two_qubit_gate_noise_depolarizing_model(fields, output, error));
    EXPECT_FALSE(error.empty());
}

// Every probability the parsers accept must produce a valid model, i.e. the
// parser bound and the model bound agree exactly.
TEST(GateNoiseDepolarizingParserTest, ParserBoundsMatchTheModelBounds)
{
    EXPECT_TRUE(SingleQubitGateNoiseModel(SingleQubitGateNoiseModel::MAX_DEPOLARIZING_PROBABILITY).is_valid());
    EXPECT_TRUE(TwoQubitGateNoiseModel(TwoQubitGateNoiseModel::MAX_DEPOLARIZING_PROBABILITY).is_valid());

    for (const auto& [text, expect_ok] : std::initializer_list<std::pair<const char*, bool>>{
             {"0.75", true}, {"0.7501", false}})
    {
        const FieldMap fields = {{"type", "DEPOLARIZING"}, {"depolarizing_probability", text}};
        GateNoiseConfigData output;
        std::string error;
        EXPECT_EQ(config_parse::parse_single_qubit_gate_noise_depolarizing_model(fields, output, error),
                  expect_ok)
            << text;
    }
}

} // namespace
