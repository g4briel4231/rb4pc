#include "rb4pc/builder/song_layout.hpp"

#include <stdexcept>
#include <string>

namespace rb4pc::builder {
std::optional<std::filesystem::path> song_output_path(std::string_view internal_path) {
    std::string normalized(internal_path);
    for (auto& character : normalized) if (character == '\\') character = '/';
    std::size_t start = 0;
    if (normalized.starts_with("songs/")) start = 6;
    else {
        const auto marker = normalized.find("/songs/");
        if (marker == std::string::npos || marker == 0) return std::nullopt;
        start = marker + 7;
    }
    std::filesystem::path result{"songs"};
    std::size_t position = start;
    while (position < normalized.size()) {
        const auto separator = normalized.find('/', position);
        const auto component = normalized.substr(position, separator - position);
        if (component.empty() || component == "." || component == ".." || component.find(':') != std::string::npos) {
            throw std::runtime_error("unsafe song path component");
        }
        result /= component;
        if (separator == std::string::npos) break;
        position = separator + 1;
    }
    if (position == normalized.size()) throw std::runtime_error("song path has no relative content");
    return result;
}
} // namespace rb4pc::builder
