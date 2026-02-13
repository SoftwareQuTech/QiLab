#pragma once

#include <string>
#include <vector>
#include <optional>

/**
 * @brief Represents a single node configuration
 */
struct NodeConfig {
    int num_qubits;
};

/**
 * @brief Application configuration loaded from YAML file
 */
struct AppConfig {
    std::vector<NodeConfig> nodes;
};

/**
 * @brief Loads configuration from YAML file
 * 
 * Reads configuration from ~/.qilab/config/nodes.yaml by default
 * or from a custom path if provided.
 * 
 * @param config_path Optional custom path to config file. If empty, uses default path.
 * @return AppConfig if successful, std::nullopt if file not found or parsing failed
 */
std::optional<AppConfig> load_config(const std::string& config_path = "");

/**
 * @brief Get the default configuration path
 * 
 * @return std::string Path to ~/.qilab/config/nodes.yaml
 */
std::string get_default_config_path();

/**
 * @brief Create a default configuration file if it doesn't exist
 * 
 * Creates ~/.qilab/config directory and a default nodes.yaml file
 * with 2 nodes, each having 10 qubits.
 * 
 * @return true if file was created successfully
 */
bool create_default_config();
