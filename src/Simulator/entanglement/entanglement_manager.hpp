/**
 * @file entanglement_manager.hpp
 * @brief Manages quantum entanglement request matching and synchronization.
 *
 * The EntanglementManager handles the lifecycle of entanglement requests between
 * quantum nodes. It matches incoming requests from different nodes, validates timing,
 * and coordinates the distribution of paired entanglement operations.
 */

#pragma once
#include <vector>
#include <chrono>
#include <memory>
#include <functional>

#include "DataTypes/commands.hpp"
#include "DataTypes/responses.hpp"

/**
 * @brief Represents a pending entanglement request from a single node.
 *
 * An entanglement request specifies that a node wishes to entangle with a neighbor node.
 * The manager collects these requests and attempts to match them bidirectionally.
 */
struct EntanglementRequest
{
    int neighbour_node;                                  ///< Target neighbor node for entanglement
    int node_id;                                         ///< Source node requesting entanglement
    std::unique_ptr<CommandBase> command;               ///< The entanglement command to execute
    std::chrono::steady_clock::time_point command_issue_time;  ///< Timestamp when request was issued
};

/**
 * @brief Represents a matched pair of entanglement requests.
 *
 * A pair is created when two nodes have requested entanglement with each other.
 * The pair holds both commands and tracks when the entanglement should be realized.
 */
struct EntanglementPair
{
    std::unique_ptr<CommandBase> command1;               ///< Entanglement command for first node
    std::unique_ptr<CommandBase> command2;               ///< Entanglement command for second node
    int node_id_1;                                       ///< ID of first node
    int node_id_2;                                       ///< ID of second node
    std::chrono::steady_clock::time_point match_time;   ///< Timestamp when pair was matched
    int calculated_success_time_delay;                  ///< Based on a model, the time to wait before executing entanglement, or fail if invalid
};

/**
 * @brief Manager for quantum entanglement request matching and execution.
 *
 * The EntanglementManager implements a request matching and queueing system for
 * quantum entanglement operations. Key responsibilities:
 *
 * 1. **Request Registration**: Accept entanglement requests from nodes
 * 2. **Bidirectional Matching**: Match requests from node A to B with requests from B to A
 * 3. **Timeout Management**: Expire requests that don't find matches within a timeout period
 * 4. **Delayed Execution**: Hold matched pairs until appropriate time, then execute both
 * 5. **Callback Dispatch**: Use callbacks to notify system of successes and failures
 *
 * The manager maintains two collections:
 * - pending_requests: Unmatched requests waiting for a partner
 * - matched_pairs: Successfully matched pairs awaiting execution time
 *
 * ### Request Lifecycle
 * 1. Node A adds request to entangle with Node B
 * 2. Manager checks if Node B has a matching request for Node A
 * 3. If matched: Move to matched_pairs with execution timestamp
 * 4. If unmatched: Keep in pending_requests, set timeout
 * 5. On timeout: Call response_callback with failure status
 * 6. On execution time: Call command_callback for each node with its command
 *
 * @note All timestamps use steady_clock for consistent monotonic timing
 * @note Timeouts are enforced per request (500 microseconds)
 * @note Matched pairs execute with a configurable delay (default 1 microsecond)
 */
class EntanglementManager
{
public:
    /**
     * @brief Constructs an EntanglementManager with callback handlers.
     *
     * @param resp_cb Callback function invoked when a request expires.
     *                Signature: void(int node_id, Outcome_Status_t status)
     *                Status is ENTANGLEMENT_SYNC_FAILURE for timeouts.
     * @param cmd_cb Callback function invoked when a matched pair executes.
     *               Signature: void(int node_id, std::unique_ptr<CommandBase> cmd)
     *               Called once for each node in the matched pair.
     */
    EntanglementManager(std::function<void(int, Outcome_Status_t)> resp_cb, 
                        std::function<void(int, std::unique_ptr<CommandBase>)> cmd_cb);

    /**
     * @brief Processes all pending requests and matched pairs.
     *
     * This method should be called regularly from the main simulation loop to:
     * - Check for expired pending requests and invoke failure callbacks
     * - Check for matched pairs that have reached execution time and invoke callbacks
     * - Clean up processed entries from both collections
     *
     * Call this method at a frequency appropriate for your timing precision needs.
     * Higher frequency improves timing accuracy but increases CPU overhead.
     *
     * @return -1 if no requests or pairs are pending (idle)
     * @return 0 on successful processing
     */
    int update_requests();

    /**
     * @brief Registers a new entanglement request.
     *
     * Accepts a new entanglement request from a node and attempts to match it
     * with an existing request from the target neighbor. If a match is found,
     * both requests are moved to the matched pairs collection.
     *
     * @param command The entanglement command to execute (moved)
     * @param node_id The node originating the request
     * @param neighbour_node The target neighbor node for entanglement
     * @return true if the request was successfully added
     * @return false if there was an error (e.g., invalid parameters)
     */
    bool add_new_request(std::unique_ptr<CommandBase> command, uint16_t node_id, uint16_t neighbour_node);

protected:
    /**
     * @brief Checks if an entanglement request has exceeded the timeout period.
     *
     * Requests that do not find a match within ENTANGLEMENT_TIMEOUT (500 microseconds)
     * are considered expired and should be failed.
     *
     * @param request The request to check
     * @return true if the request has expired and should be failed
     * @return false if the request is still valid
     */
    bool check_request_is_expired(const EntanglementRequest &request);

    /**
     * @brief Attempts to find a matching request for the given request.
     *
     * Searches pending_requests for a request where:
     * - The existing request's neighbour_node equals the given request's node_id
     * - The existing request's node_id equals the given request's neighbour_node
     *
     * This ensures bidirectional agreement: A wants B AND B wants A.
     *
     * @param request The request to match
     * @return true if a matching request was found in pending_requests
     * @return false if no matching request exists yet
     */
    bool match_request(const EntanglementRequest& request);

private:
    /// Timeout duration for unmatched entanglement requests (50000 microseconds)
    static constexpr std::chrono::microseconds ENTANGLEMENT_TIMEOUT{50000};

    /// Default execution delay after matching (100ms)
    static constexpr int DEFAULT_SUCCESS_DELAY_NS = 100000;

    /// Callback invoked when a request expires without finding a match
    std::function<void(int, Outcome_Status_t)> response_callback;

    /// Callback invoked when a matched pair reaches execution time
    std::function<void(int, std::unique_ptr<CommandBase>)> command_callback;

    /// Collection of unmatched entanglement requests awaiting partners
    std::vector<EntanglementRequest> pending_requests;

    /// Collection of matched request pairs awaiting execution time
    std::vector<EntanglementPair> matched_pairs;
};
