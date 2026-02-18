#include "config_loader.hpp"
#include <yaml-cpp/yaml.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdlib>

#include "logger/logger_api.hpp"

std::string get_default_config_path() {
    const char* explicit_path = std::getenv("QILAB_CONFIG_PATH");
    if (explicit_path && explicit_path[0] != '\0') {
        return std::string(explicit_path);
    }

    const char* config_dir = std::getenv("QILAB_CONFIG_DIR");
    if (config_dir && config_dir[0] != '\0') {
        return std::string(config_dir) + "/nodes.yaml";
    }

    const char* home = std::getenv("HOME");
    if (!home) {
        // Fallback to current directory if HOME is not set
        return ".qilab/config/nodes.yaml";
    }
    return std::string(home) + "/.qilab/config/nodes.yaml";
}

bool create_default_config() {
    std::string config_path = get_default_config_path();
    std::filesystem::path path(config_path);
    
    // Create directory if it doesn't exist
    std::filesystem::path dir = path.parent_path();
    try {
        if (!std::filesystem::exists(dir)) {
            std::filesystem::create_directories(dir);
            LOG_INFO("Created config directory: " + dir.string());
        }
        
        // Don't overwrite existing config
        if (std::filesystem::exists(config_path)) {
            LOG_INFO("Config file already exists at: " + config_path);

            return true;
        }
        
        // Create default configuration
        std::ofstream config_file(config_path);
        if (!config_file.is_open()) {
            LOG_ERROR("Failed to create config file: " + config_path);
            return false;
        }
        
        config_file << "# QILab Node Configuration\n";
        config_file << "# This file defines the quantum network nodes for simulation\n";
        config_file << "#\n";
        config_file << "# Each node should specify:\n";
        config_file << "#   num_qubits: Number of qubits in the node\n";
        config_file << "\n";
        config_file << "nodes:\n";
        config_file << "  - num_qubits: 10\n";
        config_file << "  - num_qubits: 10\n";
        
        config_file.close();
        LOG_INFO("Created default config file at: " + config_path);
        return true;
        
    } catch (const std::filesystem::filesystem_error& e) {
        LOG_ERROR("Filesystem error: " + std::string(e.what()));
        return false;
    }
}

std::optional<AppConfig> load_config(const std::string& config_path) {
    std::string path = config_path.empty() ? get_default_config_path() : config_path;
    
    // Check if file exists
    if (!std::filesystem::exists(path)) {
        LOG_ERROR("Config file not found: " + path);
        LOG_INFO("Creating default configuration...");
        if (!create_default_config()) {
            return std::nullopt;
        }
    }
    
    try {
        YAML::Node config = YAML::LoadFile(path);
        AppConfig app_config;
        
        if (!config["nodes"]) {
            LOG_ERROR("Config file is missing 'nodes' section");
            return std::nullopt;
        }
        
        const YAML::Node& nodes = config["nodes"];
        if (!nodes.IsSequence()) {
            LOG_ERROR("'nodes' must be a list");
            return std::nullopt;
        }
        
        for (const auto& node : nodes) {
            NodeConfig node_config;
            
            if (!node["num_qubits"]) {
                LOG_ERROR("Node missing 'num_qubits' field");
                return std::nullopt;
            }
            
            node_config.num_qubits = node["num_qubits"].as<int>();
            
            if (node_config.num_qubits <= 0) {
                LOG_ERROR("num_qubits must be positive, got: " + std::to_string(node_config.num_qubits));
                return std::nullopt;
            }
            
            app_config.nodes.push_back(node_config);
        }
        
        LOG_INFO("Loaded configuration with " + std::to_string(app_config.nodes.size()) 
                 + " nodes from: " + path);
        
        return app_config;
        
    } catch (const YAML::Exception& e) {
        LOG_ERROR("YAML parsing error: " + std::string(e.what()));
        return std::nullopt;
    } catch (const std::exception& e) {
        LOG_ERROR("Error loading config: " + std::string(e.what()));
        return std::nullopt;
    }
}
