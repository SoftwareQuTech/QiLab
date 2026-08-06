/*
** Statistical/probabilistic validation: run many trials of real quantum
** operations (via the real Stim-backed StimAdapterNVCenters, not a spy/mock)
** under a configured noise model -- or noiseless -- and verify the empirical
** results match theoretical expectations. This is deliberately at the
** adapter+noise-model layer (not the full command pipeline) so the
** statistics reflect genuine Stim sampling, not bookkeeping around it; the
** pipeline/routing correctness for these same operations is covered
** separately in test/Functional/Pipeline/.
**
** Statistical-test idiom matches the existing house style established in
** test/Unit/Simulator/NoiseModels/test_entanglement_attempt_noise.cpp: a
** fixed seed for reproducibility, N in the 10^4+ range, EXPECT_NEAR(empirical,
** theoretical, absolute_tolerance) against a closed-form expectation -- no
** chi-square/KS machinery, consistent with the rest of the repo.
**
** Physical setup: StimAdapterNVCenters::attempt_entanglement's initiator path
** deterministically builds |Psi+> = (|01>+|10>)/sqrt(2) (confirmed by reading
** the implementation: fixed R,R,H,X,CX circuit, Bell-state code 2). In the Z
** basis this state is PERFECTLY ANTI-CORRELATED: measuring both qubits always
** gives opposite bits. EntanglementLinkNoiseModel's Pauli channel is applied,
** per entanglement_attempt_handler, only to the initiator's own qubit via
** single_qubit_noise. An X or Y error flips that qubit's Z-basis outcome,
** which flips the pair from anti-correlated to (spuriously) correlated; a
** pure Z error does NOT change the Z-basis outcome at all. So:
**   P(qubits read EQUAL) = p_x + p_y  (== 2*p_depolarizing/3 for a pure
**   depolarizing channel with p_dephasing=0, since p_x=p_y=p_depolarizing/3)
** This gives a genuinely physically-meaningful, closed-form statistic tied
** directly to the loaded noise model, rather than an arbitrary counter.
*/

#include <gtest/gtest.h>

#include <random>

#include "Simulator/SimulatorAdapters/stim_nv_adapter.hpp"
#include "Simulator/NoiseModels/EntanglementGenerationNoise/entanglement_link_noise.hpp"

namespace {

// One (initiator-side) entanglement + noise + measure-both trial. Reinitializes
// both qubits to |0> first so each trial starts from a clean, known state.
std::pair<uint8_t, uint8_t> run_one_trial(StimAdapterNVCenters &adapter, EntanglementLinkNoiseModel *link_noise_model) {
    adapter.qubit_initialize(0);
    adapter.qubit_initialize(1);

    adapter.attempt_entanglement(/*fidelity=*/0, /*local_qubit=*/0, /*neighbour_address=*/1,
                                  /*is_initiator=*/true, /*qid=*/0);

    if (link_noise_model != nullptr) {
        const PauliChannelWeights w = link_noise_model->get_pauli_channel_weights();
        adapter.single_qubit_noise(/*qubit_address=*/0, w.q_x, w.q_y, w.q_z);
    }

    const uint8_t r0 = adapter.qubit_measure(0, false);
    const uint8_t r1 = adapter.qubit_measure(1, false);
    return {r0, r1};
}

// Noiseless: the |Psi+> Bell pair is deterministically anti-correlated in the
// Z basis on every single trial, with zero variance -- a sanity baseline the
// statistical tests below build on.
TEST(NoiseStatisticalValidationTest, NoiselessEntanglementIsAlwaysAntiCorrelated) {
    std::seed_seq seed{1, 2, 3, 4};
    StimAdapterNVCenters adapter(seed, /*num_qubits=*/2);

    constexpr int kTrials = 500;
    for (int i = 0; i < kTrials; ++i) {
        const auto [r0, r1] = run_one_trial(adapter, /*link_noise_model=*/nullptr);
        ASSERT_NE(r0, r1) << "trial " << i << ": noiseless Bell pair must be anti-correlated";
    }
}

// A pure depolarizing channel (p_dephasing=0) flips the Z-basis correlation
// at rate p_x + p_y = 2*p_depolarizing/3. N=20000 trials, seeded for
// reproducibility, tolerance matching the house style for statistics at this
// sample size.
TEST(NoiseStatisticalValidationTest, DepolarizingChannelProducesExpectedCorrelationBreakRate) {
    std::seed_seq seed{5, 6, 7, 8};
    StimAdapterNVCenters adapter(seed, /*num_qubits=*/2);

    constexpr float kPDepolarizing = 0.3f;
    EntanglementLinkNoiseModel link_noise_model(kPDepolarizing, /*p_dephasing=*/0.0f);
    ASSERT_TRUE(link_noise_model.is_valid());

    constexpr int kTrials = 20000;
    int agree_count = 0;
    for (int i = 0; i < kTrials; ++i) {
        const auto [r0, r1] = run_one_trial(adapter, &link_noise_model);
        if (r0 == r1) ++agree_count;
    }

    const double empirical_agree_rate = static_cast<double>(agree_count) / kTrials;
    const double theoretical_agree_rate = 2.0 * kPDepolarizing / 3.0; // p_x + p_y

    EXPECT_NEAR(empirical_agree_rate, theoretical_agree_rate, 0.02)
        << "empirical=" << empirical_agree_rate << " theoretical=" << theoretical_agree_rate;
}

// A pure dephasing channel (p_depolarizing=0) applies ONLY a Z error to the
// initiator's qubit, which does not change its Z-basis measurement outcome at
// all -- so even a LARGE p_dephasing must leave the pair perfectly
// anti-correlated, exactly like the noiseless case. This is a deterministic
// check (not statistical), demonstrating the physical reasoning rather than
// just asserting a rate near zero.
TEST(NoiseStatisticalValidationTest, PureDephasingNeverBreaksZBasisCorrelation) {
    std::seed_seq seed{9, 10, 11, 12};
    StimAdapterNVCenters adapter(seed, /*num_qubits=*/2);

    EntanglementLinkNoiseModel link_noise_model(/*p_depolarizing=*/0.0f, /*p_dephasing=*/0.9f);
    ASSERT_TRUE(link_noise_model.is_valid());

    constexpr int kTrials = 2000;
    for (int i = 0; i < kTrials; ++i) {
        const auto [r0, r1] = run_one_trial(adapter, &link_noise_model);
        ASSERT_NE(r0, r1) << "trial " << i << ": pure dephasing must not break Z-basis anti-correlation";
    }
}

} // namespace
