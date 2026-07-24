#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>

#include "Simulator/NoiseModels/EntanglementGenerationNoise/entanglement_attempt_noise.hpp"

namespace {

constexpr uint64_t ATTEMPT_DURATION_NS = 100;
constexpr uint32_t BATCH_SIZE = 500;
constexpr uint64_t SEED = 42;

EntanglementAttemptModel make_model(float p_success)
{
    return EntanglementAttemptModel(ATTEMPT_DURATION_NS, BATCH_SIZE, p_success, SEED);
}

// ---- Parameter validation ----

TEST(EntanglementAttemptModelTest, ValidParametersGiveValidModel)
{
    auto model = make_model(0.5f);
    EXPECT_TRUE(model.is_valid());
}

TEST(EntanglementAttemptModelTest, RejectsOutOfRangeParameters)
{
    EntanglementAttemptModel zero_duration(0, BATCH_SIZE, 0.5f, SEED);
    EntanglementAttemptModel zero_batch(ATTEMPT_DURATION_NS, 0, 0.5f, SEED);
    auto bad_success = make_model(1.5f);

    EXPECT_FALSE(zero_duration.is_valid());
    EXPECT_FALSE(zero_batch.is_valid());
    EXPECT_FALSE(bad_success.is_valid());
}

TEST(EntanglementAttemptModelTest, InvalidModelAlwaysFailsBatches)
{
    auto model = make_model(1.5f);
    ASSERT_FALSE(model.is_valid());

    const auto outcome = model.attempt_batch();
    EXPECT_FALSE(outcome.success);
    EXPECT_EQ(outcome.attempts, 0u);
    EXPECT_EQ(outcome.duration_ns, 0u);
}

// ---- Batched attempt sampling (Sec. 4.2) ----

TEST(EntanglementAttemptModelTest, CertainSuccessTakesOneAttempt)
{
    auto model = make_model(1.0f);
    const auto outcome = model.attempt_batch();
    EXPECT_TRUE(outcome.success);
    EXPECT_EQ(outcome.attempts, 1u);
    EXPECT_EQ(outcome.duration_ns, ATTEMPT_DURATION_NS);
}

TEST(EntanglementAttemptModelTest, ZeroSuccessProbabilityFailsWholeBatch)
{
    auto model = make_model(0.0f);
    const auto outcome = model.attempt_batch();
    EXPECT_FALSE(outcome.success);
    EXPECT_EQ(outcome.attempts, BATCH_SIZE);
    EXPECT_EQ(outcome.duration_ns, BATCH_SIZE * ATTEMPT_DURATION_NS);
}

TEST(EntanglementAttemptModelTest, OutcomeIsConsistent)
{
    auto model = make_model(0.01f);
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

TEST(EntanglementAttemptModelTest, SameSeedGivesSameSequence)
{
    auto model_a = make_model(0.1f);
    auto model_b = make_model(0.1f);
    for (int i = 0; i < 100; ++i)
    {
        const auto outcome_a = model_a.attempt_batch();
        const auto outcome_b = model_b.attempt_batch();
        EXPECT_EQ(outcome_a.success, outcome_b.success);
        EXPECT_EQ(outcome_a.attempts, outcome_b.attempts);
        EXPECT_EQ(outcome_a.duration_ns, outcome_b.duration_ns);
    }
}

TEST(EntanglementAttemptModelTest, MeanAttemptsMatchesGeometricDistribution)
{
    // With p_success = 0.5 and a large batch size, batch failures are
    // negligible and the mean number of attempts should approach 1/p = 2.
    const float p_success = 0.5f;
    auto model = make_model(p_success);

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

TEST(EntanglementAttemptModelTest, BatchedProcessMatchesPlainGeometricDistribution)
{
    // Eq. 8-10: resubmitting failed batches until success is probabilistically
    // identical to sampling the plain geometric distribution of Eq. 6 once.
    const float p_success = 0.3f;
    const uint32_t small_batch_size = 3; // Pr(batch fails) = 0.7^3 ~ 34%
    EntanglementAttemptModel model(ATTEMPT_DURATION_NS, small_batch_size, p_success, SEED);

    const int trials = 50000;
    const int max_tracked_k = 12;
    int counts[max_tracked_k + 1] = {};
    uint64_t total_attempts = 0;

    for (int i = 0; i < trials; ++i)
    {
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

    const double mean_attempts = static_cast<double>(total_attempts) / trials;
    EXPECT_NEAR(mean_attempts, 1.0 / p_success, 0.05);

    for (int k = 1; k <= max_tracked_k; ++k)
    {
        const double expected = p_success * std::pow(1.0 - p_success, k - 1);
        const double observed = static_cast<double>(counts[k]) / trials;
        EXPECT_NEAR(observed, expected, 0.01) << "k=" << k;
    }
}

TEST(EntanglementAttemptModelTest, SmallBatchCanFailAndResumesCorrectly)
{
    // batch_size = 1 with p = 0.5: roughly half the batches should fail.
    EntanglementAttemptModel model(ATTEMPT_DURATION_NS, 1, 0.5f, SEED);

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
