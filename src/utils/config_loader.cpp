#include "config_loader.hpp"
#include <yaml-cpp/yaml.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdlib>

std::string get_default_config_path() {
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
            std::cout << "Created config directory: " << dir << std::endl;
        }
        
        // Don't overwrite existing config
        if (std::filesystem::exists(config_path)) {
            std::cout << "Config file already exists at: " << config_path << std::endl;
            return true;
        }
        
        // Create default configuration
        std::ofstream config_file(config_path);
        if (!config_file.is_open()) {
            std::cerr << "Failed to create config file: " << config_path << std::endl;
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
        std::cout << "Created default config file at: " << config_path << std::endl;
        return true;
        
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
        return false;
    }
}

std::optional<AppConfig> load_config(const std::string& config_path) {
    std::string path = config_path.empty() ? get_default_config_path() : config_path;
    
    // Check if file exists
    if (!std::filesystem::exists(path)) {
        std::cerr << "Config file not found: " << path << std::endl;
        std::cerr << "Creating default configuration..." << std::endl;
        if (!create_default_config()) {
            return std::nullopt;
        }
    }
    
    try {
        YAML::Node config = YAML::LoadFile(path);
        AppConfig app_config;
        
        if (!config["nodes"]) {
            std::cerr << "Config file is missing 'nodes' section" << std::endl;
            return std::nullopt;
        }
        
        const YAML::Node& nodes = config["nodes"];
        if (!nodes.IsSequence()) {
            std::cerr << "'nodes' must be a list" << std::endl;
            return std::nullopt;
        }
        
        for (const auto& node : nodes) {
            NodeConfig node_config;
            
            if (!node["num_qubits"]) {
                std::cerr << "Node missing 'num_qubits' field" << std::endl;
                return std::nullopt;
            }
            
            node_config.num_qubits = node["num_qubits"].as<int>();
            
            if (node_config.num_qubits <= 0) {
                std::cerr << "num_qubits must be positive, got: " 
                         << node_config.num_qubits << std::endl;
                return std::nullopt;
            }
            
            app_config.nodes.push_back(node_config);
        }
        
        std::cout << "Loaded configuration with " << app_config.nodes.size() 
                  << " nodes from: " << path << std::endl;
        
        return app_config;
        
    } catch (const YAML::Exception& e) {
        std::cerr << "YAML parsing error: " << e.what() << std::endl;
        return std::nullopt;
    } catch (const std::exception& e) {
        std::cerr << "Error loading config: " << e.what() << std::endl;
        return std::nullopt;
    }
}
