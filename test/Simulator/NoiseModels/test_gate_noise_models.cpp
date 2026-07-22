#include <gtest/gtest.h>

#include "Simulator/NoiseModels/EntanglementGenerationNoise/depolarizing_gate_noise.hpp"

namespace {

// ---- Single-qubit gate noise ----

TEST(SingleQubitGateNoiseModelTest, ReturnsConfiguredProbability)
{
    SingleQubitGateNoiseModel model(0.1f);
    EXPECT_TRUE(model.is_valid());
    EXPECT_FLOAT_EQ(model.get_depolarizing_probability(), 0.1f);
}

TEST(SingleQubitGateNoiseModelTest, RejectsOutOfRangeProbability)
{
    SingleQubitGateNoiseModel negative_model(-0.5f);
    SingleQubitGateNoiseModel above_max_model(2.0f);

    EXPECT_FALSE(negative_model.is_valid());
    EXPECT_FALSE(above_max_model.is_valid());
    // Invalid models degrade to noiseless
    EXPECT_FLOAT_EQ(negative_model.get_depolarizing_probability(), 0.0f);
    EXPECT_FLOAT_EQ(above_max_model.get_depolarizing_probability(), 0.0f);
}

TEST(SingleQubitGateNoiseModelTest, AcceptsBoundaryProbabilities)
{
    SingleQubitGateNoiseModel zero_model(0.0f);
    SingleQubitGateNoiseModel max_model(SingleQubitGateNoiseModel::MAX_DEPOLARIZING_PROBABILITY);

    EXPECT_TRUE(zero_model.is_valid());
    EXPECT_TRUE(max_model.is_valid());
}

TEST(SingleQubitGateNoiseModelTest, FidelityConversion)
{
    // F = 1 => p = 0 (perfect gate)
    EXPECT_FLOAT_EQ(SingleQubitGateNoiseModel::p_from_fidelity(1.0f), 0.0f);
    // p = 3/2 (1 - F): F = 0.9 => p = 0.15
    EXPECT_FLOAT_EQ(SingleQubitGateNoiseModel::p_from_fidelity(0.9f), 0.15f);
    // F = 0.5 is the maximal-mixing boundary: p = 0.75
    EXPECT_FLOAT_EQ(SingleQubitGateNoiseModel::p_from_fidelity(0.5f), 0.75f);
}

TEST(SingleQubitGateNoiseModelTest, RejectsOutOfRangeFidelity)
{
    // Below maximal mixing or above 1 is rejected with -1
    EXPECT_FLOAT_EQ(SingleQubitGateNoiseModel::p_from_fidelity(0.4f), -1.0f);
    EXPECT_FLOAT_EQ(SingleQubitGateNoiseModel::p_from_fidelity(1.1f), -1.0f);
    // The rejected value also fails model construction when piped through
    SingleQubitGateNoiseModel model(SingleQubitGateNoiseModel::p_from_fidelity(0.4f));
    EXPECT_FALSE(model.is_valid());
}

TEST(SingleQubitGateNoiseModelTest, FidelityRoundTrip)
{
    // p = 3/2 (1 - F) <=> F = 1 - 2p/3 (Eq. 2)
    const float fidelity = 0.95f;
    const float p = SingleQubitGateNoiseModel::p_from_fidelity(fidelity);
    EXPECT_FLOAT_EQ(1.0f - (2.0f * p / 3.0f), fidelity);
}

// ---- Two-qubit gate noise ----

TEST(TwoQubitGateNoiseModelTest, ReturnsConfiguredProbability)
{
    TwoQubitGateNoiseModel model(0.2f);
    EXPECT_TRUE(model.is_valid());
    EXPECT_FLOAT_EQ(model.get_depolarizing_probability(), 0.2f);
}

TEST(TwoQubitGateNoiseModelTest, RejectsOutOfRangeProbability)
{
    TwoQubitGateNoiseModel negative_model(-0.5f);
    TwoQubitGateNoiseModel above_max_model(2.0f);

    EXPECT_FALSE(negative_model.is_valid());
    EXPECT_FALSE(above_max_model.is_valid());
    // Invalid models degrade to noiseless
    EXPECT_FLOAT_EQ(negative_model.get_depolarizing_probability(), 0.0f);
    EXPECT_FLOAT_EQ(above_max_model.get_depolarizing_probability(), 0.0f);
}

TEST(TwoQubitGateNoiseModelTest, AcceptsBoundaryProbabilities)
{
    TwoQubitGateNoiseModel zero_model(0.0f);
    TwoQubitGateNoiseModel max_model(TwoQubitGateNoiseModel::MAX_DEPOLARIZING_PROBABILITY);

    EXPECT_TRUE(zero_model.is_valid());
    EXPECT_TRUE(max_model.is_valid());
}

TEST(TwoQubitGateNoiseModelTest, FidelityConversion)
{
    // F = 1 => p = 0 (perfect gate)
    EXPECT_FLOAT_EQ(TwoQubitGateNoiseModel::p_from_fidelity(1.0f), 0.0f);
    // p = 5/4 (1 - F): F = 0.9 => p = 0.125
    EXPECT_FLOAT_EQ(TwoQubitGateNoiseModel::p_from_fidelity(0.9f), 0.125f);
    // F = 0.25 is the maximal-mixing boundary: p = 0.9375
    EXPECT_FLOAT_EQ(TwoQubitGateNoiseModel::p_from_fidelity(0.25f), 0.9375f);
}

TEST(TwoQubitGateNoiseModelTest, RejectsOutOfRangeFidelity)
{
    // Below maximal mixing or above 1 is rejected with -1
    EXPECT_FLOAT_EQ(TwoQubitGateNoiseModel::p_from_fidelity(0.2f), -1.0f);
    EXPECT_FLOAT_EQ(TwoQubitGateNoiseModel::p_from_fidelity(1.1f), -1.0f);
    // The rejected value also fails model construction when piped through
    TwoQubitGateNoiseModel model(TwoQubitGateNoiseModel::p_from_fidelity(0.2f));
    EXPECT_FALSE(model.is_valid());
}

TEST(TwoQubitGateNoiseModelTest, FidelityRoundTrip)
{
    // p = 5/4 (1 - F) <=> F = 1 - 4p/5 (Eq. 5)
    const float fidelity = 0.95f;
    const float p = TwoQubitGateNoiseModel::p_from_fidelity(fidelity);
    EXPECT_FLOAT_EQ(1.0f - (4.0f * p / 5.0f), fidelity);
}

} // namespace
