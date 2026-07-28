#include <gtest/gtest.h>

#include "Simulator/pauli_channel_math.hpp"

namespace {

// compose_pauli_channels(x1, y1, z1, x2, y2, z2) composes two independent
// single-qubit Pauli channels -- channel 1 (x1, y1, z1) applied first, then
// channel 2 (x2, y2, z2) -- into the equivalent combined channel. See
// src/Simulator/pauli_channel_math.hpp; this is the shared formula used by
// both gate noise (idle dephasing + gate depolarizing) and
// entanglement-link noise (Sec. 4.3 Eq. 17: link dephasing + link
// depolarizing).

// ---- Normal composition path ----

TEST(ComposePauliChannelsTest, ComposesTwoAsymmetricChannels)
{
    // Channel 1: x1 = 0.10, y1 = 0.05, z1 = 0.05 (identity1 = 1 - 0.20 = 0.80)
    // Channel 2: x2 = 0.20, y2 = 0.00, z2 = 0.10 (identity2 = 1 - 0.30 = 0.70)
    //
    // q_x = identity1*x2 + x1*identity2 + y1*z2 + z1*y2
    //     = 0.8*0.2 + 0.1*0.7 + 0.05*0.1 + 0.05*0.0 = 0.16 + 0.07 + 0.005 + 0.0 = 0.235
    // q_y = identity1*y2 + y1*identity2 + x1*z2 + z1*x2
    //     = 0.8*0.0 + 0.05*0.7 + 0.1*0.1 + 0.05*0.2 = 0.0 + 0.035 + 0.01 + 0.01 = 0.055
    // q_z = identity1*z2 + z1*identity2 + x1*y2 + y1*x2
    //     = 0.8*0.1 + 0.05*0.7 + 0.1*0.0 + 0.05*0.2 = 0.08 + 0.035 + 0.0 + 0.01 = 0.125
    //
    // sum(q_x, q_y, q_z) = 0.415 <= 1.0, so no fallback; q_i = 1 - 0.415 = 0.585
    const auto w = compose_pauli_channels(0.10f, 0.05f, 0.05f, 0.20f, 0.00f, 0.10f);

    EXPECT_NEAR(w.q_x, 0.235f, 1e-6f);
    EXPECT_NEAR(w.q_y, 0.055f, 1e-6f);
    EXPECT_NEAR(w.q_z, 0.125f, 1e-6f);
    EXPECT_NEAR(w.q_i, 0.585f, 1e-6f);
    EXPECT_NEAR(w.q_i + w.q_x + w.q_y + w.q_z, 1.0f, 1e-6f);
}

TEST(ComposePauliChannelsTest, WeightsSumToOneAcrossSeveralPoints)
{
    // A handful of non-overflowing points; the composed channel must always
    // remain a valid probability distribution.
    const float points[][6] = {
        {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
        {0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 0.0f, 0.2f, 0.0f},
        {0.05f, 0.05f, 0.05f, 0.1f, 0.1f, 0.1f},
        {0.3f, 0.0f, 0.0f, 0.0f, 0.0f, 0.3f},
    };
    for (const auto &p : points)
    {
        const auto w = compose_pauli_channels(p[0], p[1], p[2], p[3], p[4], p[5]);
        EXPECT_NEAR(w.q_i + w.q_x + w.q_y + w.q_z, 1.0f, 1e-6f)
            << "channel1=(" << p[0] << "," << p[1] << "," << p[2] << ") "
            << "channel2=(" << p[3] << "," << p[4] << "," << p[5] << ")";
    }
}

// ---- Identity channel ----

TEST(ComposePauliChannelsTest, ComposingWithZeroChannelReturnsOtherChannelUnchanged)
{
    // Channel 1 is the identity (no noise); composing with it must leave
    // channel 2's weights untouched.
    const auto w = compose_pauli_channels(0.0f, 0.0f, 0.0f, 0.15f, 0.10f, 0.05f);

    EXPECT_FLOAT_EQ(w.q_x, 0.15f);
    EXPECT_FLOAT_EQ(w.q_y, 0.10f);
    EXPECT_FLOAT_EQ(w.q_z, 0.05f);
    EXPECT_FLOAT_EQ(w.q_i, 0.70f);
}

TEST(ComposePauliChannelsTest, ComposingTwoZeroChannelsGivesIdentityChannel)
{
    const auto w = compose_pauli_channels(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    EXPECT_FLOAT_EQ(w.q_i, 1.0f);
    EXPECT_FLOAT_EQ(w.q_x, 0.0f);
    EXPECT_FLOAT_EQ(w.q_y, 0.0f);
    EXPECT_FLOAT_EQ(w.q_z, 0.0f);
}

// ---- Clamp / error branch (composed weights summing above 1.0) ----

TEST(ComposePauliChannelsTest, OverflowingCompositionFallsBackToIdentity)
{
    // Channel 1 is not itself a valid probability distribution
    // (x1 + y1 + z1 = 2.0 > 1.0, i.e. identity1 = -1.0). Composed with the
    // identity channel 2:
    //   q_x = identity1*x2 + x1*identity2 + y1*z2 + z1*y2 = -1*0 + 1*1 + 1*0 + 0*0 = 1
    //   q_y = identity1*y2 + y1*identity2 + x1*z2 + z1*x2 = -1*0 + 1*1 + 1*0 + 0*1 = 1
    //   q_z = identity1*z2 + z1*identity2 + x1*y2 + y1*x2 = -1*0 + 0*1 + 1*0 + 1*1 = 1
    // sum = 3.0 > 1.0, so the function must reject this and fall back to the
    // identity channel rather than returning the (unphysical) computed weights.
    const auto w = compose_pauli_channels(1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    EXPECT_FLOAT_EQ(w.q_i, 1.0f);
    EXPECT_FLOAT_EQ(w.q_x, 0.0f);
    EXPECT_FLOAT_EQ(w.q_y, 0.0f);
    EXPECT_FLOAT_EQ(w.q_z, 0.0f);
}

// ---- Boundary: exactly summing to 1.0 must NOT trigger the fallback ----

TEST(ComposePauliChannelsTest, ExactlyOneTotalDoesNotTriggerFallback)
{
    // Pure-X channel (x1 = 1) composed with pure-Y channel (y2 = 1):
    //   identity1 = 1 - 1 = 0, identity2 = 1 - 1 = 0
    //   q_x = identity1*x2 + x1*identity2 + y1*z2 + z1*y2 = 0
    //   q_y = identity1*y2 + y1*identity2 + x1*z2 + z1*x2 = 0
    //   q_z = identity1*z2 + z1*identity2 + x1*y2 + y1*x2 = 0*0 + 0*0 + 1*1 + 0*0 = 1
    // sum = 1.0 exactly (not > 1.0), so the composed weights must be returned
    // as-is (q_i = 0), not replaced by the identity fallback.
    const auto w = compose_pauli_channels(1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);

    EXPECT_FLOAT_EQ(w.q_z, 1.0f);
    EXPECT_FLOAT_EQ(w.q_x, 0.0f);
    EXPECT_FLOAT_EQ(w.q_y, 0.0f);
    EXPECT_FLOAT_EQ(w.q_i, 0.0f);
}

} // namespace
