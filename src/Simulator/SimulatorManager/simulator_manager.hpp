/*
 * simulator_manager.hpp
 *
 *  Created on: Apr 27, 2024
 *      Author: Milen Kolev
 *      Description: The fiel contains declaration for the manager class. 
 *                  It contains logic for calling the correct sim functions and 
 *                  managing the simulation state.
 */

#pragma once
#include "memory"
#include "array"
#include "chrono"
#include "unordered_map"

#include "DataTypes/commands.hpp"
#include "DataTypes/responses.hpp"
#include "Simulator/sim_interface.hpp"
#include "Simulator/qubit_noise_interface.hpp"
#include "Simulator/entanglement_channel_noise_interface.hpp"
#include "DataTypes/configuration_options.hpp"
#include "logger/profiler_api.hpp"
#include "Simulator/SimulatorManager/operations_engine.hpp"

class SimulatorManager
{
    using clock_type = std::chrono::steady_clock;
    using time_point = typename clock_type::time_point;
    using duration = typename clock_type::duration;
    
public:
    SimulatorManager(std::unique_ptr<ISimulator> simulator, bool qubit_noise_enabled = true, bool entanglement_noise_enabled = true);
    ~SimulatorManager();

    /**
     * @brief Configures per-link entanglement-channel noise models.
     *
     * Merges the given models into the existing set (keyed by
     * encode_pair_key(node_a, node_b)) rather than replacing it, so it may be
     * called multiple times to layer in additional links. A link with no
     * configured model has no noise applied on successful entanglement (a
     * warning is logged).
     *
     * @param models Map of link key to channel model, moved in.
     */
    void configure_entanglement_links(std::unordered_map<uint16_t, std::unique_ptr<EntanglementChannelNoiseInterface>> models);

        /**
     * @brief Register a new quantum node with the specified number of qubits.
     *
     * Nodes are registered sequentially, each receiving a unique ID based on
     * registration order. Qubits are allocated from a shared pool with per-node
     * offsets to support independent addressing. 
     * One node could have qubits with different models
     *
     * @param node_config Configuration data for the node to register
     * 
     * @return return code 0 success
     * @return -1 if registration fails (max nodes reached or insufficient qubits)
     *
     * @note Node IDs are assigned sequentially starting from 0
     * @note Qubits are allocated contiguously in global address space
     */
    int16_t register_node(const NodeConfig &node_config);

    /**
     * @brief Unregister a quantum node and free its qubits.
     *
     * @param node_id The ID of the node to unregister
     * 
     * @return true if the node was successfully unregistered
     * @return false if the node could not be unregistered (not implemented)
     *
     * @note Currently not implemented; returns false
     */
    bool unregister_node(int16_t node_id);

    int add_new_command_to_process(uint16_t node_id, std::unique_ptr<CommandBase> command, ResponseData& response);
    int try_get_ready_response(ResponseData& response, uint16_t &node_id);

    const uint16_t get_registered_nodes() const { return registered_nodes; }
    const uint16_t get_registered_qubits() const { return registered_qubits; }
private:
    struct CommandTimingInfo
    {
        uint64_t target_delay_ns = 0;
        int32_t global_qubit_1 = -1;
        int32_t global_qubit_2 = -1;
    };

    using SimCommandFn = ResponseData (SimulatorManager::*)(std::unique_ptr<CommandBase> command,
                                                            int16_t qubit_offset,
                                                            uint8_t node_id);

    /// Maximum number of handler functions that can be registered in the command dispatch table                                                          
    static constexpr int MAX_COMMANDS_AVAILABLE = 64;
    /// Maximum number of qubits that can be allocated across all nodes
    static constexpr int MAX_QUBITS_AVAILABLE = 1024;
    /// Maximum number of nodes that can be registered simultaneously
    static constexpr int MAX_NODES_AVAILABLE = 4;

    /// Registry of nodes. Index is node_id; value is (qubit_offset, qubit_count) pair
    /// Qubit offset is the starting address for this node's qubits in global space
    std::array<std::pair<uint16_t, uint16_t>, MAX_NODES_AVAILABLE> nodes;
    uint16_t registered_nodes = 0; // Number of currently registered nodes

    /// Total number of qubits already allocated across all nodes
    uint16_t registered_qubits = 0;
    std::array<std::unique_ptr<QubitNoiseInterface>, MAX_QUBITS_AVAILABLE> simulation_qubits;

    /// Per-pair cache of the 4-way Bell state identified by the initiator of an
    /// entanglement pair (the lower node id, which is dispatched and processed
    /// first). The responder reads and erases the entry so both nodes report the
    /// same Bell state, without the simulator adapter having to hold any per-pair
    /// coordination state. Keyed by encode_pair_key(node_a, node_b).
    std::unordered_map<uint16_t, int8_t> pair_bell_states;

    /// Build an order-independent key for a node pair: (min << 8) | max.
    static uint16_t encode_pair_key(uint8_t node_a, uint8_t node_b);

    /// Per-link entanglement-channel noise models, keyed by encode_pair_key(node_a, node_b).
    std::unordered_map<uint16_t, std::unique_ptr<EntanglementChannelNoiseInterface>> entanglement_channel_models;

    /// Pending basis-rotation (pre-measurement) gates for a node, set by a
    /// PRE_MEASUREMENT_GATES_SELECTION command and consumed by the next matching
    /// measurement. Kept per node (and per target) in the manager -- NOT in the
    /// adapter -- so one node's selection can never be applied to another node's
    /// qubit when commands interleave across the shared simulator.
    struct PreMeasureGates
    {
        bool enabled = false;
        Angle_t angle1{};
        Angle_t angle2{};
        Angle_t angle3{};
    };

    /// [node_id][target]: target 0 = regular qubit_measure, 1 = entanglement measure.
    std::array<std::array<PreMeasureGates, 2>, MAX_NODES_AVAILABLE> pre_measure_gates{};

    /// Apply this node's pending pre-measurement rotation (if any) to @p global_qubit
    /// via single_qubit_gate, then clear it (one-shot). @p target selects the slot.
    void apply_pre_measure_gates(uint8_t node_id, uint16_t global_qubit, bool target);

    /// Resolve the 4-way Bell state to report for one side of a matched pair:
    /// the initiator caches its freshly identified state; the responder reads the
    /// cached value (and erases it). Falls back to @p local_bell on a cache miss.
    int8_t resolve_pair_bell_state(uint8_t node_id, uint8_t neighbour_id, bool is_initiator, int8_t local_bell);

    /// Registry of delays per node. Keep it separate in order to keep the nodes array as ligh as possible
    /// The array index is equal to the node index
    std::array<QuantumOperationsDelaysNs, MAX_NODES_AVAILABLE> node_operation_delays;
    /// The underlying quantum simulator
    std::unique_ptr<ISimulator> simulator_inst;

    /// Gates qubit-in-memory noise application in single_qubit_gate_handler, from system.qubit_noise_enabled config
    bool qubit_noise_enabled = true;

    /// Gates entanglement-channel noise application in entanglement_attempt_handler/entanglement_measure_handler, from system.entanglement_noise_enabled config
    bool entanglement_noise_enabled = true;

    std::array<SimCommandFn, MAX_COMMANDS_AVAILABLE> sim_command_table = {};

    //operations engine functions
    OperationsEngine operations_engine;

    bool is_local_qubit_in_range(uint16_t node_id, uint16_t local_qubit) const;
    bool are_local_qubits_in_range(uint16_t node_id, uint16_t local_qubit1, uint16_t local_qubit2) const;
    CommandTimingInfo build_command_timing_info(uint16_t node_id, int16_t qubit_offset, const CommandBase &command) const;
    void apply_touch_with_delay(const CommandTimingInfo &timing_info, uint64_t wait_ns);
    
    ResponseData nop_command_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id);
    ResponseData single_qubit_gate_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id);
    ResponseData two_qubit_gate_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id);
    ResponseData qubit_measure_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id);
    ResponseData qubit_init_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id);
    ResponseData entanglement_attempt_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id);
    ResponseData entanglement_measure_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id);
    ResponseData entanglement_swap_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id);
    ResponseData pre_measurement_gates_selection_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id);
    ResponseData qubit_move_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id);
    ResponseData qubit_swap_handler(std::unique_ptr<CommandBase> command, int16_t qubit_offset, uint8_t node_id);


        /// Profiling event IDs for performance measurement

    /// Time to execute any command type
    Profiler_id prof_event_execute_command = PROF_REGISTER_EVENT("execute_command");

    /// Time for no-op (placeholder) commands
    Profiler_id prof_event_command_nop = PROF_REGISTER_EVENT("command_nop");

    /// Time to initialize/reset qubits
    Profiler_id prof_event_qubit_init = PROF_REGISTER_EVENT("qubit_init");

    /// Time to measure qubits
    Profiler_id prof_event_qubit_measurement = PROF_REGISTER_EVENT("qubit_measurement");

    /// Time to apply single-qubit gates
    Profiler_id prof_event_single_qubit_gate = PROF_REGISTER_EVENT("single_qubit_gate");

    /// Time to apply two-qubit gates
    Profiler_id prof_event_two_qubit_gate = PROF_REGISTER_EVENT("two_qubit_gate");

    /// Time to swap qubits
    Profiler_id prof_event_qubit_swap = PROF_REGISTER_EVENT("qubit_swap");

    /// Time to attempt entanglement operation
    Profiler_id prof_event_entanglement_attempt = PROF_REGISTER_EVENT("entanglement_attempt");

    /// Time to perform entanglement swap
    Profiler_id prof_event_entanglement_swap = PROF_REGISTER_EVENT("entanglement_swap");

    /// Time to measure entangled qubits
    Profiler_id prof_event_entanglement_measure = PROF_REGISTER_EVENT("entanglement_measure");

    /// Time for pre-measurement basis selection
    Profiler_id prof_event_pre_measure_select = PROF_REGISTER_EVENT("pre_measure_select");

    /// Time to move qubits between nodes
    Profiler_id prof_event_qubit_move = PROF_REGISTER_EVENT("qubit_move");


};
