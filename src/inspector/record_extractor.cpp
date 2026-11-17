#include "rb4pc/inspector/record_extractor.hpp"

#include "rb4pc/inspector/archive_layout.hpp"
#include "rb4pc/inspector/file_inspector.hpp"
#include "rb4pc/inspector/sha256.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <stdexcept>

namespace rb4pc::inspector {

ExtractionResult extract_record(const HeaderSetReport& report,
                                std::string_view internal_path,
                                const std::filesystem::path& destination) {
    if (destination.empty()) throw std::runtime_error("extraction destination is empty");
    if (std::filesystem::exists(destination)) {
        throw std::runtime_error("extraction destination already exists: " + destination.string());
    }
    const auto parent = destination.has_parent_path() ? destination.parent_path() : std::filesystem::path{"."};
    if (!std::filesystem::is_directory(parent)) {
        throw std::runtime_error("extraction destination directory does not exist: " + parent.string());
    }

    const auto record_index = find_file_record(report.metadata, internal_path);
    if (!record_index) throw std::runtime_error("internal path was not found: " + std::string(internal_path));
    if (*record_index >= report.record_locations.size() || !report.record_locations[*record_index]) {
        throw std::runtime_error("record does not resolve to one ARK volume");
    }
    const auto location = *report.record_locations[*record_index];
    if (location.archive_index >= report.archives.size() || !report.archives[location.archive_index].exists) {
        throw std::runtime_error("record's ARK volume is missing");
    }
    const auto& record = report.metadata.file_records[*record_index];
    const auto& archive = report.archives[location.archive_index];
    if (location.local_offset > archive.actual_size || record.size > archive.actual_size - location.local_offset) {
        throw std::runtime_error("record exceeds the actual ARK file size");
    }

    auto temporary = destination;
    temporary += ".rb4pc-part";
    if (std::filesystem::exists(temporary)) {
        throw std::runtime_error("temporary extraction path already exists: " + temporary.string());
    }

    try {
        std::ifstream input(archive.path, std::ios::binary);
        if (!input) throw std::runtime_error("could not open record's ARK volume");
        input.seekg(static_cast<std::streamoff>(location.local_offset));
        if (!input) throw std::runtime_error("could not seek to record in ARK volume");
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("could not create temporary extraction file");

        std::array<char, 64 * 1024> buffer{};
        std::uint64_t remaining = record.size;
        while (remaining != 0) {
            const auto chunk = static_cast<std::size_t>(std::min<std::uint64_t>(remaining, buffer.size()));
            input.read(buffer.data(), static_cast<std::streamsize>(chunk));
            if (static_cast<std::size_t>(input.gcount()) != chunk) {
                throw std::runtime_error("unexpected end of ARK during extraction");
            }
            output.write(buffer.data(), static_cast<std::streamsize>(chunk));
            if (!output) throw std::runtime_error("could not write temporary extraction file");
            remaining -= chunk;
        }
        output.close();
        if (!output) throw std::runtime_error("could not finalize temporary extraction file");
        std::filesystem::rename(temporary, destination);
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }

    return {destination, record.size, sha256_file(destination)};
}

} // namespace rb4pc::inspector
