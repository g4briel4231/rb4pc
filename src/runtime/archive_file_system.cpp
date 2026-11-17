#include "rb4pc/runtime/archive_file_system.hpp"

#include "rb4pc/inspector/archive_layout.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace rb4pc::runtime {
namespace { constexpr std::size_t maximum_single_read = 64 * 1024 * 1024; }

ArchiveFileSystem::ArchiveFileSystem(inspector::HeaderSetReport report) : report_(std::move(report)) {}

ArchiveFileSystem ArchiveFileSystem::mount(const std::filesystem::path& header) {
    return ArchiveFileSystem(inspector::inspect_header_set(header));
}

bool ArchiveFileSystem::contains(std::string_view path) const {
    return inspector::find_file_record(report_.metadata, path).has_value();
}

std::optional<std::uint64_t> ArchiveFileSystem::file_size(std::string_view path) const {
    const auto index = inspector::find_file_record(report_.metadata, path);
    if (!index) return std::nullopt;
    return report_.metadata.file_records[*index].size;
}

std::vector<std::byte> ArchiveFileSystem::read(std::string_view path, std::uint64_t offset,
                                               std::size_t size) const {
    if (size > maximum_single_read) throw std::runtime_error("VFS read exceeds 64 MiB safety limit");
    const auto index = inspector::find_file_record(report_.metadata, path);
    if (!index) throw std::runtime_error("VFS path was not found");
    const auto& record = report_.metadata.file_records[*index];
    if (offset > record.size || size > static_cast<std::uint64_t>(record.size) - offset) {
        throw std::runtime_error("VFS read exceeds record bounds");
    }
    if (*index >= report_.record_locations.size() || !report_.record_locations[*index]) {
        throw std::runtime_error("VFS record has no ARK location");
    }
    const auto location = *report_.record_locations[*index];
    const auto& archive = report_.archives[location.archive_index];
    std::ifstream input(archive.path, std::ios::binary);
    if (!input) throw std::runtime_error("VFS could not open ARK volume");
    input.seekg(static_cast<std::streamoff>(location.local_offset + offset));
    std::vector<std::byte> result(size);
    input.read(reinterpret_cast<char*>(result.data()), static_cast<std::streamsize>(size));
    if (static_cast<std::size_t>(input.gcount()) != size) throw std::runtime_error("VFS short read");
    return result;
}

std::size_t ArchiveFileSystem::file_count() const { return report_.metadata.file_records.size(); }

std::vector<std::string> ArchiveFileSystem::list_paths(std::string_view prefix, std::size_t limit) const {
    std::vector<std::string> result;
    if (limit == 0) return result;
    result.reserve(std::min(limit, report_.metadata.file_records.size()));
    for (const auto& record : report_.metadata.file_records) {
        if (record.path.starts_with(prefix)) {
            result.push_back(record.path);
            if (result.size() == limit) break;
        }
    }
    return result;
}
} // namespace rb4pc::runtime
