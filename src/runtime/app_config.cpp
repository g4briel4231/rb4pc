#include "rb4pc/runtime/app_config.hpp"

#include <cmath>
#include <fstream>
#include <stdexcept>
#include <string>

namespace rb4pc::runtime {
namespace {
void validate(const AppConfig& config) {
    if (!std::isfinite(config.master_volume) || config.master_volume < 0.0F ||
        config.master_volume > 1.0F)
        throw std::runtime_error("master_volume must be between 0.0 and 1.0");
    if (config.latency_ms < -250 || config.latency_ms > 250)
        throw std::runtime_error("latency_ms must be between -250 and 250");
}
} // namespace

AppConfig load_app_config(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("could not open RB4PC configuration: " + path.string());
    AppConfig config;
    bool found_volume = false;
    bool found_latency = false;
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty() || line.front() == '#') continue;
        const auto separator = line.find('=');
        if (separator == std::string::npos) throw std::runtime_error("malformed RB4PC configuration line");
        const auto key = line.substr(0, separator);
        const auto value = line.substr(separator + 1);
        try {
            if (key == "master_volume") {
                if (found_volume) throw std::runtime_error("duplicate master_volume setting");
                std::size_t consumed = 0;
                config.master_volume = std::stof(value, &consumed);
                if (consumed != value.size()) throw std::runtime_error("invalid master_volume setting");
                found_volume = true;
            } else if (key == "latency_ms") {
                if (found_latency) throw std::runtime_error("duplicate latency_ms setting");
                std::size_t consumed = 0;
                config.latency_ms = std::stoi(value, &consumed);
                if (consumed != value.size()) throw std::runtime_error("invalid latency_ms setting");
                found_latency = true;
            } else {
                throw std::runtime_error("unknown RB4PC configuration setting: " + key);
            }
        } catch (const std::invalid_argument&) {
            throw std::runtime_error("invalid value in RB4PC configuration");
        } catch (const std::out_of_range&) {
            throw std::runtime_error("out-of-range value in RB4PC configuration");
        }
    }
    if (!found_volume || !found_latency)
        throw std::runtime_error("RB4PC configuration is missing a required setting");
    validate(config);
    return config;
}

AppConfig load_or_create_app_config(const std::filesystem::path& path) {
    if (std::filesystem::exists(path)) return load_app_config(path);
    const AppConfig config;
    save_app_config(path, config);
    return config;
}

void save_app_config(const std::filesystem::path& path, const AppConfig& config) {
    validate(config);
    if (path.empty()) throw std::runtime_error("RB4PC configuration path is empty");
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("could not write RB4PC configuration: " + path.string());
    output << "# RB4PC user settings\n"
           << "master_volume=" << config.master_volume << "\n"
           << "latency_ms=" << config.latency_ms << "\n";
    if (!output) throw std::runtime_error("could not finish RB4PC configuration");
}

} // namespace rb4pc::runtime
