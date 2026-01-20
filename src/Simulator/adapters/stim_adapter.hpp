/*
** Created on: 22.08.2025
** Created by: mrkolev
** Description: Contains a mock adapter in order to do initial testing of adapter timings
*/
#pragma once
#include <random>

#include "stim.h"
#include "Simulator/sim_interface.hpp"
#include "logger/logger.hpp"

/**
 * @brief Adapter implementing ISimulator on top of the Stim tableau simulator.
 *
 * This mock adapter is intended for early timing/perf experiments and functional
 * bring-up. It wraps a @c stim::TableauSimulator with fixed word packing
 * (64 bits) and exposes the project’s simulator interface.
 *
 * @par Design notes
 * - The adapter focuses on determinism and reproducibility via an injected RNG seed.
 * - Logging is optional and non-owning; use it for perf/timing traces.
 * - Entanglement-* methods are dummies/mocks unless otherwise specified in impl.
 *
 * @par Thread-safety
 * Instances are not inherently thread-safe. Guard externally if accessed
 * from multiple threads.
 */
class StimAdapter : public ISimulator
{
public:
    /**
     * @brief Construct a Stim-backed simulator.
     *
     * @param seed_seq Seed sequence used to initialize the internal RNG.
     * @param num_qubits Number of logical qubits to allocate/track.
     *
     * @note The internal Stim tableau is sized/configured to accommodate
     *       @p num_qubits. Behavior for operations outside this range is
     *       implementation-defined (typically rejected).
     */
    StimAdapter(std::seed_seq &seed_seq, const uint8_t &num_qubits);

    /// @brief Virtual destructor to allow cleanup via base pointer.
    ~StimAdapter() override;

    // ===== ISimulator interface =====

    /**
     * @copydoc ISimulator::qubit_initialize
     */
    bool qubit_initialize(uint8_t qubit_address) override;

    /**
     * @copydoc ISimulator::single_qubit_gate
     *
     * @details Mapped to Stim single-qubit tableau operations. The mapping
     *          from (axis, angle) to a concrete Stim gate is defined by the
     *          implementation
     */
    bool single_qubit_gate(uint8_t qubit_address, Angle_t angle, Axis_t axis) override;

    /**
     * @copydoc ISimulator::two_qubit_gate
     *
     * @details Implementations typically realize CX/CZ/iSWAP via Stim’s
     *          tableau updates. Control/target convention is documented in impl.
     */
    bool two_qubit_gate(uint8_t qubit1_address, uint8_t qubit2_address, Angle_t angle, Axis_t axis) override;

    /**
     * @copydoc ISimulator::qubit_measure
     *
     * @details Uses Stim measurement; basis selection is mapped internally
     *          (e.g., pre-rotations for X-basis).
     */
    uint8_t qubit_measure(uint8_t qubit_address, bool basis) override;

    /**
     * @brief Mock entanglement attempt.
     *
     * Generates a synthetic status based on @p fidelity and RNG. This method
     * exists for early integration and timing; replace with a proper link-layer
     * model as needed.
     *
     * @param fidelity Target fidelity parameter guiding mock success rate.
     * @param neighbour_address Peer/link identifier (unused in pure mock).
     * @param qid Local qubit identifier.
     * @return Implementation-defined status byte (e.g., 0=success).
     *
     * @note Consider aligning return coding with the rest of the API via a
     *       strong enum in the future.
     */
    uint8_t attempt_entanglement(uint16_t fidelity, uint8_t local_qubit, uint8_t neighbour_address, uint8_t qid) override;

    /**
     * @copydoc ISimulator::entanglement_measure
     *
     * @details Typically mocked: optionally performs @p repetitions attempts
     *          when @p try_now is true and returns a synthesized outcome.
     */
    uint8_t entanglement_measure(uint16_t fidelity, uint8_t neighbour_address, bool basis,
                                 uint8_t qid, bool try_now, uint8_t repetitions) override;

    /**
     * @copydoc ISimulator::pre_measure_gates_selection
     *
     * @details Stores a policy used to inject pre-measurement single-qubit
     *          rotations in subsequent @c qubit_measure calls.
     */
    void pre_measure_gates_selection(bool target, Angle_t angle_1, Angle_t angle_2, Angle_t angle_3) override;

    /**
     * @copydoc ISimulator::qubit_move
     */
    void qubit_move(uint8_t qubit1_address, uint8_t qubit2_address) override;

    /**
     * @copydoc ISimulator::qubit_swap
     */
    void qubit_swap(uint8_t qubit1_address, uint8_t qubit2_address) override;

    /**
     * @copydoc ISimulator::entanglement_swap
     *
     * @details Mocked entanglement swapping; updates internal bookkeeping and
     *          returns a status code.
     */
    uint8_t entanglement_swap(uint8_t qubit1_address, uint8_t qubit2_address) override;

    bool allocate_qubits(uint8_t num_qubits) override;
private:
    // Cached objects for all quantum operations
    
    // For single-target operations (measure, initialize, single-qubit gates)
    std::array<stim::GateTarget, 1> single_target;
    stim::SpanRef<stim::GateTarget> single_span;
    
    // Measurement specific
    stim::CircuitInstruction measure_inst;
    
    // Initialization specific
    stim::CircuitInstruction init_inst;
    
    // Single qubit gate specific

    stim::CircuitInstruction x_gate_inst;       // X
    stim::CircuitInstruction sqrt_x_gate_inst;       // SQRT_X
    stim::CircuitInstruction x_gate_dag_inst;   // SQRT_X_DAG
    stim::CircuitInstruction y_gate_inst;       // Y
    stim::CircuitInstruction sqrt_y_gate_inst;       // SQRT_Y
    stim::CircuitInstruction y_gate_dag_inst;   // SQRT_Y_DAG
    stim::CircuitInstruction z_gate_inst;       // Z
    stim::CircuitInstruction sqrt_z_gate_inst;       // S (SQRT_Z) 
    stim::CircuitInstruction z_gate_dag_inst;   // S_DAG (SQRT_Z_DAG)
    
    // For two-qubit operations (CNOT, CZ, SWAP)
    std::array<stim::GateTarget, 2> two_target;
    stim::SpanRef<stim::GateTarget> two_span;
    stim::CircuitInstruction cnot_inst;
    stim::CircuitInstruction cz_inst;
    stim::CircuitInstruction swap_inst;
    
    // Threshold for measurement record clearing
    static constexpr size_t MEASUREMENT_CLEAR_THRESHOLD = 1024;
    
    // Cache control for initialization
    static constexpr bool USE_CACHED_INSTRUCTIONS = true;
    
    static constexpr uint8_t MAX_QUBITS_AVAILABLE = 250;
    
    uint64_t registered_qubits = 0;
    /**
     * @brief Pseudo-random number generator for mock outcomes.
     *
     * Seeded from the provided @c std::seed_seq in the constructor to enable
     * reproducible tests.
     */
    std::mt19937_64 generator;

    /**
     * @brief Underlying Stim tableau simulator instance.
     *
     * The template argument (64) defines the internal bit-packing width. You
     * may experiment with other packings to evaluate performance trade-offs.
     */
    stim::TableauSimulator<64> stim_instance;

    // Non-copyable and non-movable: each adapter owns unique simulator state.
    StimAdapter(const StimAdapter &) = delete;
    StimAdapter(const StimAdapter &&) = delete;
    StimAdapter &operator=(StimAdapter &) = delete;
    StimAdapter &operator=(StimAdapter &&) = delete;
};
