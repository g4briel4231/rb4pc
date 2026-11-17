#pragma once

#include "rb4pc/inspector/file_inspector.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace rb4pc::runtime {

class ArchiveFileSystem {
public:
    explicit ArchiveFileSystem(inspector::HeaderSetReport report);
    [[nodiscard]] static ArchiveFileSystem mount(const std::filesystem::path& header);
    [[nodiscard]] bool contains(std::string_view internal_path) const;
    [[nodiscard]] std::optional<std::uint64_t> file_size(std::string_view internal_path) const;
    [[nodiscard]] std::vector<std::byte> read(
        std::string_view internal_path, std::uint64_t offset, std::size_t size) const;
    [[nodiscard]] std::size_t file_count() const;
    [[nodiscard]] std::vector<std::string> list_paths(
        std::string_view prefix, std::size_t limit) const;

private:
    inspector::HeaderSetReport report_;
};

} // namespace rb4pc::runtime
