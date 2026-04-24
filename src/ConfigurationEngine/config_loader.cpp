#include "config_loader.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <charconv>
#include <cctype>

#include "logger/logger_api.hpp"

namespace {

std::string trim(const std::string& input) noexcept {
    size_t begin = 0;
    while (begin < input.size() && std::isspace(static_cast<unsigned char>(input[begin])) != 0) {
        ++begin;
    }

    size_t end = input.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(input[end - 1])) != 0) {
        --end;
    }

    return input.substr(begin, end - begin);
}

bool parse_positive_int(const std::string& text, int& value) noexcept {
    const std::string trimmed = trim(text);
    if (trimmed.empty()) {
        return false;
    }

    int parsed_value = 0;
    const char* begin = trimmed.data();
    const char* end = trimmed.data() + trimmed.size();
    const auto [ptr, ec] = std::from_chars(begin, end, parsed_value);
    if (ec != std::errc() || ptr != end || parsed_value <= 0) {
        return false;
    }

    value = parsed_value;
    return true;
}

bool parse_node_num_qubits_line(const std::string& line, int& num_qubits) noexcept {
    static constexpr const char* key = "num_qubits:";
    const std::string trimmed = trim(line);

    if (trimmed.rfind(key, 0) != 0) {
        return false;
    }

    return parse_positive_int(trimmed.substr(11), num_qubits);
}

} // namespace

std::string get_default_config_path() noexcept {
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

bool create_default_config() noexcept {
    std::string config_path = get_default_config_path();
    std::filesystem::path path(config_path);
    
    // Create directory if it doesn't exist
    std::filesystem::path dir = path.parent_path();
    std::error_code error;
    if (!std::filesystem::exists(dir, error)) {
        if (error) {
            LOG_ERROR("Failed to inspect config directory '" + dir.string() + "': " + error.message());
            return false;
        }

        if (!std::filesystem::create_directories(dir, error)) {
            if (error) {
                LOG_ERROR("Failed to create config directory '" + dir.string() + "': " + error.message());
                return false;
            }
        } else {
            LOG_INFO("Created config directory: " + dir.string());
        }
    }

    // Don't overwrite existing config
    if (std::filesystem::exists(config_path, error)) {
        if (error) {
            LOG_ERROR("Failed to inspect config file '" + config_path + "': " + error.message());
            return false;
        }

        LOG_INFO("Config file already exists at: " + config_path);
        return true;
    }

    if (error) {
        LOG_ERROR("Failed to inspect config file '" + config_path + "': " + error.message());
        return false;
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
}

std::optional<AppConfig> load_config(const std::string& config_path) noexcept {
    std::string path = config_path.empty() ? get_default_config_path() : config_path;
    
    // Check if file exists
    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        if (error) {
            LOG_ERROR("Failed to inspect config file '" + path + "': " + error.message());
            return std::nullopt;
        }

        LOG_ERROR("Config file not found: " + path);
        LOG_INFO("Creating default configuration...");
        if (!create_default_config()) {
            return std::nullopt;
        }
    }

    std::ifstream config_file(path);
    if (!config_file.is_open()) {
        LOG_ERROR("Failed to open config file: " + path);
        return std::nullopt;
    }

    AppConfig app_config;

    bool found_nodes_section = false;
    bool pending_node_value = false;
    std::string line;

    while (std::getline(config_file, line)) {
        const std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        if (!found_nodes_section) {
            if (trimmed == "nodes:") {
                found_nodes_section = true;
            }
            continue;
        }

        if (pending_node_value) {
            int parsed_num_qubits = 0;
            if (!parse_node_num_qubits_line(trimmed, parsed_num_qubits)) {
                LOG_ERROR("Node missing or invalid 'num_qubits' field");
                return std::nullopt;
            }

            app_config.nodes.push_back(NodeConfig{.num_qubits = parsed_num_qubits});
            pending_node_value = false;
            continue;
        }

        if (trimmed[0] == '-') {
            const std::string after_dash = trim(trimmed.substr(1));
            if (after_dash.empty()) {
                pending_node_value = true;
                continue;
            }

            int parsed_num_qubits = 0;
            if (!parse_node_num_qubits_line(after_dash, parsed_num_qubits)) {
                LOG_ERROR("Node missing or invalid 'num_qubits' field");
                return std::nullopt;
            }

            app_config.nodes.push_back(NodeConfig{.num_qubits = parsed_num_qubits});
            continue;
        }

        if (trimmed.back() == ':') {
            break;
        }

        LOG_ERROR("Invalid nodes list entry: " + trimmed);
        return std::nullopt;
    }

    if (!found_nodes_section) {
        LOG_ERROR("Config file is missing 'nodes' section");
        return std::nullopt;
    }

    if (pending_node_value) {
        LOG_ERROR("Node missing 'num_qubits' field");
        return std::nullopt;
    }

    if (app_config.nodes.empty()) {
        LOG_ERROR("'nodes' must be a non-empty list");
        return std::nullopt;
    }

    LOG_INFO("Loaded configuration with " + std::to_string(app_config.nodes.size())
             + " nodes from: " + path);

    return app_config;
}
