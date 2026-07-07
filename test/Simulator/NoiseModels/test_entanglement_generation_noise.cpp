#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>

#include "Simulator/NoiseModels/entanglement_generation_noise.hpp"

namespace {

constexpr uint64_t ATTEMPT_DURATION_NS = 100;
constexpr uint32_t BATCH_SIZE = 500;
constexpr uint64_t SEED = 42;

EntanglementGenerationModel make_model(float p_success, float p_depolarizing, float p_dephasing)
{
    return EntanglementGenerationModel(ATTEMPT_DURATION_NS, BATCH_SIZE, p_success,
                                       p_depolarizing, p_dephasing, SEED);
}

// ---- Parameter validation ----

TEST(EntanglementGenerationModelTest, ValidParametersGiveValidModel)
{
    auto model = make_model(0.5f, 0.1f, 0.2f);
    EXPECT_TRUE(model.is_valid());
}

TEST(EntanglementGenerationModelTest, RejectsOutOfRangeParameters)
{
    EntanglementGenerationModel zero_duration(0, BATCH_SIZE, 0.5f, 0.0f, 0.0f, SEED);
    EntanglementGenerationModel zero_batch(ATTEMPT_DURATION_NS, 0, 0.5f, 0.0f, 0.0f, SEED);
    auto bad_success = make_model(1.5f, 0.0f, 0.0f);
    auto bad_depolarizing = make_model(0.5f, -0.1f, 0.0f);
    auto bad_dephasing = make_model(0.5f, 0.0f, 2.0f);

    EXPECT_FALSE(zero_duration.is_valid());
    EXPECT_FALSE(zero_batch.is_valid());
    EXPECT_FALSE(bad_success.is_valid());
    EXPECT_FALSE(bad_depolarizing.is_valid());
    EXPECT_FALSE(bad_dephasing.is_valid());
}

TEST(EntanglementGenerationModelTest, InvalidModelFailsBatchesAndIsNoiseless)
{
    auto model = make_model(1.5f, 0.0f, 0.0f);
    ASSERT_FALSE(model.is_valid());

    const auto outcome = model.attempt_batch();
    EXPECT_FALSE(outcome.success);
    EXPECT_EQ(outcome.attempts, 0u);
    EXPECT_EQ(outcome.duration_ns, 0u);

    const auto w = model.get_pauli_channel_weights();
    EXPECT_FLOAT_EQ(w.q_i, 1.0f);
    EXPECT_FLOAT_EQ(w.q_x, 0.0f);
    EXPECT_FLOAT_EQ(w.q_y, 0.0f);
    EXPECT_FLOAT_EQ(w.q_z, 0.0f);
}

// ---- Pauli channel weights (Eq. 17) ----

TEST(EntanglementGenerationModelTest, WeightsSumToOne)
{
    const float test_points[][2] = {{0.0f, 0.0f}, {0.1f, 0.0f}, {0.0f, 0.2f}, {0.3f, 0.4f}, {1.0f, 1.0f}};
    for (const auto &point : test_points)
    {
        auto model = make_model(0.5f, point[0], point[1]);
        const auto w = model.get_pauli_channel_weights();
        EXPECT_NEAR(w.q_i + w.q_x + w.q_y + w.q_z, 1.0f, 1e-6f)
            << "p_d=" << point[0] << " p_z=" << point[1];
    }
}

TEST(EntanglementGenerationModelTest, NoNoiseGivesIdentityChannel)
{
    auto model = make_model(0.5f, 0.0f, 0.0f);
    const auto w = model.get_pauli_channel_weights();
    EXPECT_FLOAT_EQ(w.q_i, 1.0f);
    EXPECT_FLOAT_EQ(w.q_x, 0.0f);
    EXPECT_FLOAT_EQ(w.q_y, 0.0f);
    EXPECT_FLOAT_EQ(w.q_z, 0.0f);
}

TEST(EntanglementGenerationModelTest, PureDepolarizingHasUniformPauliWeights)
{
    // p_z = 0 recovers pure depolarizing: q_x = q_y = q_z = p_d / 3
    const float p_d = 0.3f;
    auto model = make_model(0.5f, p_d, 0.0f);
    const auto w = model.get_pauli_channel_weights();
    EXPECT_FLOAT_EQ(w.q_x, p_d / 3.0f);
    EXPECT_FLOAT_EQ(w.q_y, p_d / 3.0f);
    EXPECT_FLOAT_EQ(w.q_z, p_d / 3.0f);
    EXPECT_FLOAT_EQ(w.q_i, 1.0f - p_d);
}

TEST(EntanglementGenerationModelTest, PureDephasingHasOnlyZWeight)
{
    // p_d = 0 recovers pure dephasing: q_z = p_z, q_x = q_y = 0
    const float p_z = 0.25f;
    auto model = make_model(0.5f, 0.0f, p_z);
    const auto w = model.get_pauli_channel_weights();
    EXPECT_FLOAT_EQ(w.q_x, 0.0f);
    EXPECT_FLOAT_EQ(w.q_y, 0.0f);
    EXPECT_FLOAT_EQ(w.q_z, p_z);
    EXPECT_FLOAT_EQ(w.q_i, 1.0f - p_z);
}

TEST(EntanglementGenerationModelTest, CombinedChannelIsBiasedTowardsZ)
{
    // With both channels active, Z carries the extra dephasing share (Eq. 17)
    const float p_d = 0.2f;
    const float p_z = 0.1f;
    auto model = make_model(0.5f, p_d, p_z);
    const auto w = model.get_pauli_channel_weights();
    EXPECT_FLOAT_EQ(w.q_x, p_d / 3.0f);
    EXPECT_FLOAT_EQ(w.q_y, p_d / 3.0f);
    EXPECT_FLOAT_EQ(w.q_z, p_z * (1.0f - p_d) + (p_d / 3.0f) * (1.0f - p_z));
    EXPECT_GT(w.q_z, w.q_x);
}

// ---- Batched attempt sampling (Sec. 4.2) ----

TEST(EntanglementGenerationModelTest, CertainSuccessTakesOneAttempt)
{
    auto model = make_model(1.0f, 0.0f, 0.0f);
    const auto outcome = model.attempt_batch();
    EXPECT_TRUE(outcome.success);
    EXPECT_EQ(outcome.attempts, 1u);
    EXPECT_EQ(outcome.duration_ns, ATTEMPT_DURATION_NS);
}

TEST(EntanglementGenerationModelTest, ZeroSuccessProbabilityFailsWholeBatch)
{
    auto model = make_model(0.0f, 0.0f, 0.0f);
    const auto outcome = model.attempt_batch();
    EXPECT_FALSE(outcome.success);
    EXPECT_EQ(outcome.attempts, BATCH_SIZE);
    EXPECT_EQ(outcome.duration_ns, BATCH_SIZE * ATTEMPT_DURATION_NS);
}

TEST(EntanglementGenerationModelTest, OutcomeIsConsistent)
{
    auto model = make_model(0.01f, 0.0f, 0.0f);
    for (int i = 0; i < 1000; ++i)
    {
        const auto outcome = model.attempt_batch();
        EXPECT_GE(outcome.attempts, 1u);
        EXPECT_LE(outcome.attempts, BATCH_SIZE);
        EXPECT_EQ(outcome.duration_ns, static_cast<uint64_t>(outcome.attempts) * ATTEMPT_DURATION_NS);
        if (!outcome.success)
        {
            EXPECT_EQ(outcome.attempts, BATCH_SIZE);
        }
    }
}

TEST(EntanglementGenerationModelTest, SameSeedGivesSameSequence)
{
    auto model_a = make_model(0.1f, 0.0f, 0.0f);
    auto model_b = make_model(0.1f, 0.0f, 0.0f);
    for (int i = 0; i < 100; ++i)
    {
        const auto outcome_a = model_a.attempt_batch();
        const auto outcome_b = model_b.attempt_batch();
        EXPECT_EQ(outcome_a.success, outcome_b.success);
        EXPECT_EQ(outcome_a.attempts, outcome_b.attempts);
        EXPECT_EQ(outcome_a.duration_ns, outcome_b.duration_ns);
    }
}

TEST(EntanglementGenerationModelTest, MeanAttemptsMatchesGeometricDistribution)
{
    // With p_success = 0.5 and a large batch size, batch failures are
    // negligible and the mean number of attempts should approach 1/p = 2.
    const float p_success = 0.5f;
    auto model = make_model(p_success, 0.0f, 0.0f);

    const int trials = 20000;
    uint64_t total_attempts = 0;
    for (int i = 0; i < trials; ++i)
    {
        const auto outcome = model.attempt_batch();
        ASSERT_TRUE(outcome.success);
        total_attempts += outcome.attempts;
    }

    const double mean_attempts = static_cast<double>(total_attempts) / trials;
    EXPECT_NEAR(mean_attempts, 1.0 / p_success, 0.05);
}

TEST(EntanglementGenerationModelTest, BatchedProcessMatchesPlainGeometricDistribution)
{
    // Eq. 8-10: resubmitting failed batches until success is probabilistically
    // identical to sampling the plain geometric distribution of Eq. 6 once.
    // Run the full batched protocol (loop attempt_batch() until success,
    // accumulating attempts across failed batches) with a small batch size so
    // batch failures are frequent, and compare the distribution of the total
    // attempt count against the geometric pmf Pr(k) = p (1-p)^(k-1).
    const float p_success = 0.3f;
    const uint32_t small_batch_size = 3; // Pr(batch fails) = 0.7^3 ~ 34%
    EntanglementGenerationModel model(ATTEMPT_DURATION_NS, small_batch_size,
                                      p_success, 0.0f, 0.0f, SEED);

    const int trials = 50000;
    const int max_tracked_k = 12;
    int counts[max_tracked_k + 1] = {};
    uint64_t total_attempts = 0;

    for (int i = 0; i < trials; ++i)
    {
        // Full batched protocol: resubmit until success (QNodeOS behavior)
        uint64_t attempts_this_trial = 0;
        BatchOutcome outcome{};
        do
        {
            outcome = model.attempt_batch();
            attempts_this_trial += outcome.attempts;
        } while (!outcome.success);

        total_attempts += attempts_this_trial;
        if (attempts_this_trial <= max_tracked_k)
        {
            ++counts[attempts_this_trial];
        }
    }

    // Mean must match the geometric expectation 1/p
    const double mean_attempts = static_cast<double>(total_attempts) / trials;
    EXPECT_NEAR(mean_attempts, 1.0 / p_success, 0.05);

    // The pmf must match Pr(k) = p (1-p)^(k-1) pointwise, including across
    // batch boundaries (k = 3 -> 4 and k = 6 -> 7 span failed batches).
    for (int k = 1; k <= max_tracked_k; ++k)
    {
        const double expected = p_success * std::pow(1.0 - p_success, k - 1);
        const double observed = static_cast<double>(counts[k]) / trials;
        EXPECT_NEAR(observed, expected, 0.01) << "k=" << k;
    }
}

TEST(EntanglementGenerationModelTest, SmallBatchCanFailAndResumesCorrectly)
{
    // batch_size = 1 with p = 0.5: roughly half the batches should fail.
    EntanglementGenerationModel model(ATTEMPT_DURATION_NS, 1, 0.5f, 0.0f, 0.0f, SEED);

    const int trials = 20000;
    int failures = 0;
    for (int i = 0; i < trials; ++i)
    {
        const auto outcome = model.attempt_batch();
        EXPECT_EQ(outcome.attempts, 1u);
        EXPECT_EQ(outcome.duration_ns, ATTEMPT_DURATION_NS);
        if (!outcome.success)
        {
            ++failures;
        }
    }

    const double failure_rate = static_cast<double>(failures) / trials;
    EXPECT_NEAR(failure_rate, 0.5, 0.02); // Pr(fail) = (1 - p)^batch_size = 0.5 (Eq. 8)
}

} // namespace
