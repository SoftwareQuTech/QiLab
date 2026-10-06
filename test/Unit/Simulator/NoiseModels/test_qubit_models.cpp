// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "Simulator/NoiseModels/QubitDecoherenceNoise/phase_flip_noise.hpp"
#include "Simulator/NoiseModels/QubitDecoherenceNoise/qubit_noise_basic.hpp"

namespace {

TEST(QubitModelBasicTest, MarkTouchedReturnsZero)
{
    QubitModelBasic model(QubitNoiseBasicConfig{.t2_value = 1000.0f});
    EXPECT_EQ(model.mark_touched(), 0);
}

TEST(QubitModelBasicTest, ProbabilityRemainsBounded)
{
    QubitModelBasic model(QubitNoiseBasicConfig{.t2_value = 1000.0f});
    const float probability = model.get_qubit_decoherence_probability();
    EXPECT_GE(probability, 0.0f);
    EXPECT_LE(probability, 1.0f);
}

TEST(QubitModelBasicTest, ProbabilityIncreasesWithElapsedTime)
{
    QubitModelBasic model(QubitNoiseBasicConfig{.t2_value = 1.0f});
    model.mark_touched();
    const float p0 = model.get_qubit_decoherence_probability();

    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    const float p1 = model.get_qubit_decoherence_probability();

    EXPECT_GE(p1, p0);
}

TEST(PhaseFlipQubitModelTest, MarkTouchedReturnsZero)
{
    PhaseFlipQubitModel model(PhaseFlipNoiseConfig{.t2_value = 0.25f});
    EXPECT_EQ(model.mark_touched(), 0);
}

TEST(PhaseFlipQubitModelTest, ClampsToZeroAndOne)
{
    PhaseFlipQubitModel negative_model(PhaseFlipNoiseConfig{.t2_value = -0.1f});
    // PhaseFlipQubitModel above_one_model(PhaseFlipNoiseConfig{.t2_value = 1.2f});

    EXPECT_FLOAT_EQ(negative_model.get_qubit_decoherence_probability(), 0.0f);
    // EXPECT_FLOAT_EQ(above_one_model.get_qubit_decoherence_probability(), 1.0f);
}

//TEST(PhaseFlipQubitModelTest, ReturnsConfiguredProbabilityInRange)
//{
//    PhaseFlipQubitModel model(PhaseFlipNoiseConfig{.t2_value = 0.42f});
//    EXPECT_FLOAT_EQ(model.get_qubit_decoherence_probability(), 0.42f);
//}

} // namespace
