// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "ConfigurationEngine/parse_scalar_helpers.hpp"

// Direct unit tests for the shared scalar-parsing helpers in
// parse_scalar_helpers.hpp: trim, split_key_value, parse_positive_int,
// parse_non_negative_float, parse_bool, parse_non_negative_u16,
// parse_non_negative_u64, require_key, ensure_allowed_keys.

namespace {

using config_parse::FieldMap;

// ---------------------------------------------------------------------------
// trim
// ---------------------------------------------------------------------------

TEST(ScalarHelpersTest, TrimRemovesLeadingAndTrailingWhitespace)
{
    EXPECT_EQ(config_parse::trim("  hello  "), "hello");
    EXPECT_EQ(config_parse::trim("\t\nvalue\r\n"), "value");
}

TEST(ScalarHelpersTest, TrimLeavesInteriorWhitespaceAlone)
{
    EXPECT_EQ(config_parse::trim("  a b c  "), "a b c");
}

TEST(ScalarHelpersTest, TrimOfAllWhitespaceIsEmpty)
{
    EXPECT_EQ(config_parse::trim("   "), "");
}

TEST(ScalarHelpersTest, TrimOfEmptyStringIsEmpty)
{
    EXPECT_EQ(config_parse::trim(""), "");
}

// ---------------------------------------------------------------------------
// split_key_value
// ---------------------------------------------------------------------------

TEST(ScalarHelpersTest, SplitKeyValueParsesSimplePair)
{
    const auto result = config_parse::split_key_value("type: BASIC");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->first, "type");
    EXPECT_EQ(result->second, "BASIC");
}

TEST(ScalarHelpersTest, SplitKeyValueTrimsWhitespaceAroundKeyAndValue)
{
    const auto result = config_parse::split_key_value("   t2_value   :   1000   ");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->first, "t2_value");
    EXPECT_EQ(result->second, "1000");
}

TEST(ScalarHelpersTest, SplitKeyValueReturnsNulloptWithoutColon)
{
    const auto result = config_parse::split_key_value("no_colon_here");
    EXPECT_FALSE(result.has_value());
}

TEST(ScalarHelpersTest, SplitKeyValueReturnsNulloptWhenKeyIsEmpty)
{
    const auto result = config_parse::split_key_value("   : value");
    EXPECT_FALSE(result.has_value());
}

TEST(ScalarHelpersTest, SplitKeyValueAllowsEmptyValue)
{
    const auto result = config_parse::split_key_value("key:");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->first, "key");
    EXPECT_EQ(result->second, "");
}

TEST(ScalarHelpersTest, SplitKeyValueSplitsOnFirstColonOnly)
{
    // A value that itself contains a colon (e.g. a URL or timestamp) should
    // keep everything after the first colon as part of the value.
    const auto result = config_parse::split_key_value("key: a:b:c");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->first, "key");
    EXPECT_EQ(result->second, "a:b:c");
}

// ---------------------------------------------------------------------------
// parse_positive_int
// ---------------------------------------------------------------------------

TEST(ScalarHelpersTest, ParsePositiveIntAcceptsValidValue)
{
    int value = 0;
    EXPECT_TRUE(config_parse::parse_positive_int("42", value));
    EXPECT_EQ(value, 42);
}

TEST(ScalarHelpersTest, ParsePositiveIntAcceptsWhitespacePadded)
{
    int value = 0;
    EXPECT_TRUE(config_parse::parse_positive_int("  7  ", value));
    EXPECT_EQ(value, 7);
}

TEST(ScalarHelpersTest, ParsePositiveIntRejectsZero)
{
    int value = -1;
    EXPECT_FALSE(config_parse::parse_positive_int("0", value));
}

TEST(ScalarHelpersTest, ParsePositiveIntRejectsNegative)
{
    int value = -1;
    EXPECT_FALSE(config_parse::parse_positive_int("-4", value));
}

TEST(ScalarHelpersTest, ParsePositiveIntRejectsEmptyString)
{
    int value = -1;
    EXPECT_FALSE(config_parse::parse_positive_int("", value));
}

TEST(ScalarHelpersTest, ParsePositiveIntRejectsNonNumeric)
{
    int value = -1;
    EXPECT_FALSE(config_parse::parse_positive_int("abc", value));
}

TEST(ScalarHelpersTest, ParsePositiveIntRejectsFloatValue)
{
    // from_chars<int> stops at '.', leaving trailing characters -- rejected.
    int value = -1;
    EXPECT_FALSE(config_parse::parse_positive_int("2.5", value));
}

TEST(ScalarHelpersTest, ParsePositiveIntRejectsTrailingGarbage)
{
    int value = -1;
    EXPECT_FALSE(config_parse::parse_positive_int("5abc", value));
}

// ---------------------------------------------------------------------------
// parse_non_negative_float
// ---------------------------------------------------------------------------

TEST(ScalarHelpersTest, ParseNonNegativeFloatAcceptsValidValue)
{
    float value = -1.0f;
    EXPECT_TRUE(config_parse::parse_non_negative_float("1234.5", value));
    EXPECT_FLOAT_EQ(value, 1234.5f);
}

TEST(ScalarHelpersTest, ParseNonNegativeFloatAcceptsZero)
{
    float value = -1.0f;
    EXPECT_TRUE(config_parse::parse_non_negative_float("0", value));
    EXPECT_FLOAT_EQ(value, 0.0f);
}

TEST(ScalarHelpersTest, ParseNonNegativeFloatAcceptsWhitespacePadded)
{
    float value = -1.0f;
    EXPECT_TRUE(config_parse::parse_non_negative_float("  3.5  ", value));
    EXPECT_FLOAT_EQ(value, 3.5f);
}

TEST(ScalarHelpersTest, ParseNonNegativeFloatRejectsNegative)
{
    float value = -1.0f;
    EXPECT_FALSE(config_parse::parse_non_negative_float("-100", value));
}

TEST(ScalarHelpersTest, ParseNonNegativeFloatRejectsEmptyString)
{
    float value = -1.0f;
    EXPECT_FALSE(config_parse::parse_non_negative_float("", value));
}

TEST(ScalarHelpersTest, ParseNonNegativeFloatRejectsNonNumeric)
{
    float value = -1.0f;
    EXPECT_FALSE(config_parse::parse_non_negative_float("not_a_number", value));
}

TEST(ScalarHelpersTest, ParseNonNegativeFloatRejectsTrailingGarbage)
{
    float value = -1.0f;
    EXPECT_FALSE(config_parse::parse_non_negative_float("100abc", value));
}

TEST(ScalarHelpersTest, ParseNonNegativeFloatRejectsInfinity)
{
    // strtof parses "inf"/"infinity" successfully, but the helper explicitly
    // rejects non-finite results via std::isfinite().
    float value = -1.0f;
    EXPECT_FALSE(config_parse::parse_non_negative_float("inf", value));
    EXPECT_FALSE(config_parse::parse_non_negative_float("infinity", value));
}

TEST(ScalarHelpersTest, ParseNonNegativeFloatRejectsNaN)
{
    float value = -1.0f;
    EXPECT_FALSE(config_parse::parse_non_negative_float("nan", value));
}

TEST(ScalarHelpersTest, ParseNonNegativeFloatAcceptsScientificNotation)
{
    float value = -1.0f;
    EXPECT_TRUE(config_parse::parse_non_negative_float("1.5e3", value));
    EXPECT_FLOAT_EQ(value, 1500.0f);
}

// ---------------------------------------------------------------------------
// parse_bool
// ---------------------------------------------------------------------------

TEST(ScalarHelpersTest, ParseBoolAcceptsTrue)
{
    bool value = false;
    EXPECT_TRUE(config_parse::parse_bool("true", value));
    EXPECT_TRUE(value);
}

TEST(ScalarHelpersTest, ParseBoolAcceptsFalse)
{
    bool value = true;
    EXPECT_TRUE(config_parse::parse_bool("false", value));
    EXPECT_FALSE(value);
}

TEST(ScalarHelpersTest, ParseBoolAcceptsWhitespacePadded)
{
    bool value = false;
    EXPECT_TRUE(config_parse::parse_bool("  true  ", value));
    EXPECT_TRUE(value);
}

TEST(ScalarHelpersTest, ParseBoolRejectsYesNoStyleValues)
{
    bool value = false;
    EXPECT_FALSE(config_parse::parse_bool("yes", value));
    EXPECT_FALSE(config_parse::parse_bool("no", value));
    EXPECT_FALSE(config_parse::parse_bool("1", value));
    EXPECT_FALSE(config_parse::parse_bool("0", value));
}

TEST(ScalarHelpersTest, ParseBoolIsCaseSensitive)
{
    bool value = false;
    EXPECT_FALSE(config_parse::parse_bool("True", value));
    EXPECT_FALSE(config_parse::parse_bool("TRUE", value));
}

TEST(ScalarHelpersTest, ParseBoolRejectsEmptyString)
{
    bool value = false;
    EXPECT_FALSE(config_parse::parse_bool("", value));
}

// ---------------------------------------------------------------------------
// parse_non_negative_u16
// ---------------------------------------------------------------------------

TEST(ScalarHelpersTest, ParseNonNegativeU16AcceptsValidValue)
{
    uint16_t value = 0;
    EXPECT_TRUE(config_parse::parse_non_negative_u16("1000", value));
    EXPECT_EQ(value, 1000);
}

TEST(ScalarHelpersTest, ParseNonNegativeU16AcceptsZero)
{
    uint16_t value = 1;
    EXPECT_TRUE(config_parse::parse_non_negative_u16("0", value));
    EXPECT_EQ(value, 0);
}

TEST(ScalarHelpersTest, ParseNonNegativeU16AcceptsMaxValue)
{
    uint16_t value = 0;
    EXPECT_TRUE(config_parse::parse_non_negative_u16("65535", value));
    EXPECT_EQ(value, 65535);
}

TEST(ScalarHelpersTest, ParseNonNegativeU16RejectsOverflow)
{
    uint16_t value = 0;
    EXPECT_FALSE(config_parse::parse_non_negative_u16("65536", value));
}

TEST(ScalarHelpersTest, ParseNonNegativeU16RejectsLargeOverflow)
{
    uint16_t value = 0;
    EXPECT_FALSE(config_parse::parse_non_negative_u16("999999999", value));
}

TEST(ScalarHelpersTest, ParseNonNegativeU16RejectsNegative)
{
    // from_chars<unsigned int> fails outright on a leading '-'.
    uint16_t value = 0;
    EXPECT_FALSE(config_parse::parse_non_negative_u16("-1", value));
}

TEST(ScalarHelpersTest, ParseNonNegativeU16RejectsEmptyString)
{
    uint16_t value = 0;
    EXPECT_FALSE(config_parse::parse_non_negative_u16("", value));
}

TEST(ScalarHelpersTest, ParseNonNegativeU16RejectsNonNumeric)
{
    uint16_t value = 0;
    EXPECT_FALSE(config_parse::parse_non_negative_u16("abc", value));
}

TEST(ScalarHelpersTest, ParseNonNegativeU16RejectsTrailingGarbage)
{
    uint16_t value = 0;
    EXPECT_FALSE(config_parse::parse_non_negative_u16("100abc", value));
}

// ---------------------------------------------------------------------------
// parse_non_negative_u64
// ---------------------------------------------------------------------------

TEST(ScalarHelpersTest, ParseNonNegativeU64AcceptsValidValue)
{
    uint64_t value = 0;
    EXPECT_TRUE(config_parse::parse_non_negative_u64("1000", value));
    EXPECT_EQ(value, 1000u);
}

TEST(ScalarHelpersTest, ParseNonNegativeU64AcceptsZero)
{
    uint64_t value = 1;
    EXPECT_TRUE(config_parse::parse_non_negative_u64("0", value));
    EXPECT_EQ(value, 0u);
}

TEST(ScalarHelpersTest, ParseNonNegativeU64AcceptsLargeValue)
{
    uint64_t value = 0;
    EXPECT_TRUE(config_parse::parse_non_negative_u64("18446744073709551615", value));
    EXPECT_EQ(value, UINT64_MAX);
}

TEST(ScalarHelpersTest, ParseNonNegativeU64RejectsOverflow)
{
    // One past UINT64_MAX -- from_chars itself reports result_out_of_range.
    uint64_t value = 0;
    EXPECT_FALSE(config_parse::parse_non_negative_u64("18446744073709551616", value));
}

TEST(ScalarHelpersTest, ParseNonNegativeU64RejectsNegative)
{
    uint64_t value = 0;
    EXPECT_FALSE(config_parse::parse_non_negative_u64("-1", value));
}

TEST(ScalarHelpersTest, ParseNonNegativeU64RejectsEmptyString)
{
    uint64_t value = 0;
    EXPECT_FALSE(config_parse::parse_non_negative_u64("", value));
}

TEST(ScalarHelpersTest, ParseNonNegativeU64RejectsNonNumeric)
{
    uint64_t value = 0;
    EXPECT_FALSE(config_parse::parse_non_negative_u64("not_a_number", value));
}

TEST(ScalarHelpersTest, ParseNonNegativeU64RejectsTrailingGarbage)
{
    uint64_t value = 0;
    EXPECT_FALSE(config_parse::parse_non_negative_u64("42xyz", value));
}

// ---------------------------------------------------------------------------
// require_key
// ---------------------------------------------------------------------------

TEST(ScalarHelpersTest, RequireKeyReturnsValueWhenPresent)
{
    const FieldMap fields = {{"t2_value", "1000"}};
    std::string value;
    std::string error;

    EXPECT_TRUE(config_parse::require_key(fields, "t2_value", value, error));
    EXPECT_EQ(value, "1000");
    EXPECT_TRUE(error.empty());
}

TEST(ScalarHelpersTest, RequireKeyFailsWhenMissingAndSetsError)
{
    const FieldMap fields = {{"other_key", "1000"}};
    std::string value;
    std::string error;

    EXPECT_FALSE(config_parse::require_key(fields, "t2_value", value, error));
    EXPECT_FALSE(error.empty());
    EXPECT_NE(error.find("t2_value"), std::string::npos);
}

TEST(ScalarHelpersTest, RequireKeyAllowsEmptyStringValue)
{
    // An explicitly empty value is still "present" -- require_key only
    // checks for key existence, not non-empty content.
    const FieldMap fields = {{"t2_value", ""}};
    std::string value = "unset";
    std::string error;

    EXPECT_TRUE(config_parse::require_key(fields, "t2_value", value, error));
    EXPECT_EQ(value, "");
}

// ---------------------------------------------------------------------------
// ensure_allowed_keys
// ---------------------------------------------------------------------------

TEST(ScalarHelpersTest, EnsureAllowedKeysAcceptsSubsetOfAllowedKeys)
{
    const FieldMap fields = {{"type", "BASIC"}, {"t2_value", "1000"}};
    std::string error;

    EXPECT_TRUE(config_parse::ensure_allowed_keys(fields, {"type", "t2_value"}, error));
    EXPECT_TRUE(error.empty());
}

TEST(ScalarHelpersTest, EnsureAllowedKeysAcceptsEmptyFieldMap)
{
    const FieldMap fields;
    std::string error;

    EXPECT_TRUE(config_parse::ensure_allowed_keys(fields, {"type", "t2_value"}, error));
}

TEST(ScalarHelpersTest, EnsureAllowedKeysRejectsUnknownKeyAndSetsError)
{
    const FieldMap fields = {{"type", "BASIC"}, {"t2_value", "1000"}, {"extra_field", "1"}};
    std::string error;

    EXPECT_FALSE(config_parse::ensure_allowed_keys(fields, {"type", "t2_value"}, error));
    EXPECT_FALSE(error.empty());
    EXPECT_NE(error.find("extra_field"), std::string::npos);
}

TEST(ScalarHelpersTest, EnsureAllowedKeysRejectsWhenAllowedListIsEmpty)
{
    const FieldMap fields = {{"type", "BASIC"}};
    std::string error;

    EXPECT_FALSE(config_parse::ensure_allowed_keys(fields, {}, error));
}

} // namespace
