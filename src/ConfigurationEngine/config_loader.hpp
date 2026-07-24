#pragma once

#include <string>
#include <vector>
#include <optional>

#include "DataTypes/configuration_options.hpp"

    /**
 * @brief Application configuration loaded from YAML file
 */
struct AppConfig {
    std::vector<EntanglementLinkConfig> entanglement_links;
    SystemConfig system;
    std::vector<NodeConfig> nodes;
};

/**
 * @brief Loads configuration from YAML file
 * 
 * Path resolution order:
 * 1) Explicit function argument
 * 2) QILAB_CONFIG_PATH (full file path)
 * 3) QILAB_CONFIG_DIR + /nodes.yaml
 * 4) ~/.qilab/config/nodes.yaml (default)
 * 
 * @param config_path Optional custom path to config file. If empty, uses default path.
 * @return AppConfig if successful, std::nullopt if file not found or parsing failed
 */
std::optional<AppConfig> load_config(const std::string& config_path = "") noexcept;

/**
 * @brief Get the default configuration path
 * 
 * Resolution order:
 * - QILAB_CONFIG_PATH
 * - QILAB_CONFIG_DIR + /nodes.yaml
 * - ~/.qilab/config/nodes.yaml
 *
 * @return std::string resolved config file path
 */
std::string get_default_config_path() noexcept;

/**
 * @brief Create a default configuration file if it doesn't exist
 * 
 * Creates ~/.qilab/config directory and a default nodes.yaml file
 * with 2 nodes, each having 10 qubits.
 * 
 * @return true if file was created successfully
 */
bool create_default_config() noexcept;
