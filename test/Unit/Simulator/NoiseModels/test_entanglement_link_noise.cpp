// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <gtest/gtest.h>

#include "Simulator/NoiseModels/EntanglementGenerationNoise/entanglement_link_noise.hpp"

namespace {

EntanglementLinkNoiseModel make_model(float p_depolarizing, float p_dephasing)
{
    return EntanglementLinkNoiseModel(p_depolarizing, p_dephasing);
}

// ---- Parameter validation ----

TEST(EntanglementLinkNoiseModelTest, ValidParametersGiveValidModel)
{
    auto model = make_model(0.1f, 0.2f);
    EXPECT_TRUE(model.is_valid());
}

TEST(EntanglementLinkNoiseModelTest, RejectsOutOfRangeParameters)
{
    auto bad_depolarizing = make_model(-0.1f, 0.0f);
    auto bad_dephasing = make_model(0.0f, 2.0f);

    EXPECT_FALSE(bad_depolarizing.is_valid());
    EXPECT_FALSE(bad_dephasing.is_valid());
}

TEST(EntanglementLinkNoiseModelTest, InvalidModelIsNoiseless)
{
    auto model = make_model(1.5f, 0.0f);
    ASSERT_FALSE(model.is_valid());

    const auto w = model.get_pauli_channel_weights();
    EXPECT_FLOAT_EQ(w.q_i, 1.0f);
    EXPECT_FLOAT_EQ(w.q_x, 0.0f);
    EXPECT_FLOAT_EQ(w.q_y, 0.0f);
    EXPECT_FLOAT_EQ(w.q_z, 0.0f);
}

// ---- Pauli channel weights (Eq. 17) ----

TEST(EntanglementLinkNoiseModelTest, WeightsSumToOne)
{
    const float test_points[][2] = {{0.0f, 0.0f}, {0.1f, 0.0f}, {0.0f, 0.2f}, {0.3f, 0.4f}, {1.0f, 1.0f}};
    for (const auto &point : test_points)
    {
        auto model = make_model(point[0], point[1]);
        const auto w = model.get_pauli_channel_weights();
        EXPECT_NEAR(w.q_i + w.q_x + w.q_y + w.q_z, 1.0f, 1e-6f)
            << "p_d=" << point[0] << " p_z=" << point[1];
    }
}

TEST(EntanglementLinkNoiseModelTest, NoNoiseGivesIdentityChannel)
{
    auto model = make_model(0.0f, 0.0f);
    const auto w = model.get_pauli_channel_weights();
    EXPECT_FLOAT_EQ(w.q_i, 1.0f);
    EXPECT_FLOAT_EQ(w.q_x, 0.0f);
    EXPECT_FLOAT_EQ(w.q_y, 0.0f);
    EXPECT_FLOAT_EQ(w.q_z, 0.0f);
}

TEST(EntanglementLinkNoiseModelTest, PureDepolarizingHasUniformPauliWeights)
{
    // p_z = 0 recovers pure depolarizing: q_x = q_y = q_z = p_d / 3
    const float p_d = 0.3f;
    auto model = make_model(p_d, 0.0f);
    const auto w = model.get_pauli_channel_weights();
    EXPECT_FLOAT_EQ(w.q_x, p_d / 3.0f);
    EXPECT_FLOAT_EQ(w.q_y, p_d / 3.0f);
    EXPECT_FLOAT_EQ(w.q_z, p_d / 3.0f);
    EXPECT_FLOAT_EQ(w.q_i, 1.0f - p_d);
}

TEST(EntanglementLinkNoiseModelTest, PureDephasingHasOnlyZWeight)
{
    // p_d = 0 recovers pure dephasing: q_z = p_z, q_x = q_y = 0
    const float p_z = 0.25f;
    auto model = make_model(0.0f, p_z);
    const auto w = model.get_pauli_channel_weights();
    EXPECT_FLOAT_EQ(w.q_x, 0.0f);
    EXPECT_FLOAT_EQ(w.q_y, 0.0f);
    EXPECT_FLOAT_EQ(w.q_z, p_z);
    EXPECT_FLOAT_EQ(w.q_i, 1.0f - p_z);
}

TEST(EntanglementLinkNoiseModelTest, CombinedChannelIsBiasedTowardsZ)
{
    // With both channels active, Z carries the extra dephasing share (Eq. 17)
    const float p_d = 0.2f;
    const float p_z = 0.1f;
    auto model = make_model(p_d, p_z);
    const auto w = model.get_pauli_channel_weights();
    EXPECT_FLOAT_EQ(w.q_x, p_d / 3.0f);
    EXPECT_FLOAT_EQ(w.q_y, p_d / 3.0f);
    EXPECT_FLOAT_EQ(w.q_z, p_z * (1.0f - p_d) + (p_d / 3.0f) * (1.0f - p_z));
    EXPECT_GT(w.q_z, w.q_x);
}

} // namespace
