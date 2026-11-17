#include "rb4pc/inspector/record_extractor.hpp"

#include "rb4pc/inspector/archive_layout.hpp"
#include "rb4pc/inspector/file_inspector.hpp"
#include "rb4pc/inspector/sha256.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace rb4pc::inspector {
namespace {

std::string escape_json(std::string_view value) {
    std::ostringstream output;
    for (const unsigned char character : value) {
        if (character == '"' || character == '\\') output << '\\' << static_cast<char>(character);
        else if (character < 0x20) {
            output << "\\u00" << "0123456789abcdef"[character >> 4]
                   << "0123456789abcdef"[character & 0x0f];
        } else output << static_cast<char>(character);
    }
    return output.str();
}

std::filesystem::path safe_relative_path(std::string_view internal_path) {
    if (internal_path.empty()) throw std::runtime_error("empty internal path cannot be extracted");
    std::filesystem::path result;
    std::string component;
    const auto append_component = [&] {
        if (component.empty() || component == "." || component == ".." || component.find(':') != std::string::npos) {
            throw std::runtime_error("unsafe internal path: " + std::string(internal_path));
        }
        result /= component;
        component.clear();
    };
    for (const char character : internal_path) {
        if (character == '/' || character == '\\') append_component();
        else component.push_back(character);
    }
    append_component();
    if (result.is_absolute() || result.has_root_name() || result.has_root_directory()) {
        throw std::runtime_error("unsafe absolute internal path: " + std::string(internal_path));
    }
    return result;
}

bool matches_options(const HdrFileRecord& record, const SelectiveExtractionOptions& options) {
    if (record.size == 0) return false;
    if (options.prefix && !record.path.starts_with(*options.prefix)) return false;
    if (options.extension && !record.path.ends_with(*options.extension)) return false;
    return true;
}

} // namespace

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

SelectiveExtractionResult extract_records(const HeaderSetReport& report,
                                           const SelectiveExtractionOptions& options,
                                           const std::filesystem::path& destination_directory) {
    if ((!options.prefix || options.prefix->empty()) && (!options.extension || options.extension->empty())) {
        throw std::runtime_error("selective extraction requires a non-empty prefix or extension");
    }
    if (destination_directory.empty()) throw std::runtime_error("extraction directory is empty");
    if (std::filesystem::exists(destination_directory)) {
        throw std::runtime_error("extraction directory already exists: " + destination_directory.string());
    }
    const auto parent = destination_directory.has_parent_path()
                            ? destination_directory.parent_path()
                            : std::filesystem::path{"."};
    if (!std::filesystem::is_directory(parent)) {
        throw std::runtime_error("extraction parent directory does not exist: " + parent.string());
    }
    auto staging = destination_directory;
    staging += ".rb4pc-part";
    if (std::filesystem::exists(staging)) {
        throw std::runtime_error("temporary extraction directory already exists: " + staging.string());
    }

    struct ManifestEntry {
        std::string internal_path;
        std::string relative_path;
        ExtractionResult extraction;
    };
    std::vector<ManifestEntry> entries;
    try {
        std::filesystem::create_directory(staging);
        for (const auto& record : report.metadata.file_records) {
            if (!matches_options(record, options)) continue;
            const auto relative = safe_relative_path(record.path);
            const auto output_path = staging / relative;
            std::filesystem::create_directories(output_path.parent_path());
            const auto extraction = extract_record(report, record.path, output_path);
            entries.push_back({record.path, relative.generic_string(), extraction});
        }
        if (entries.empty()) throw std::runtime_error("no non-empty records matched the extraction filters");

        const auto manifest_path = staging / "rb4pc-manifest.json";
        std::ofstream manifest(manifest_path, std::ios::binary | std::ios::trunc);
        if (!manifest) throw std::runtime_error("could not create extraction manifest");
        manifest << "{\n  \"schemaVersion\": 1,\n  \"files\": [";
        for (std::size_t index = 0; index < entries.size(); ++index) {
            const auto& entry = entries[index];
            manifest << (index == 0 ? "\n" : ",\n")
                     << "    {\"internalPath\": \"" << escape_json(entry.internal_path)
                     << "\", \"relativePath\": \"" << escape_json(entry.relative_path)
                     << "\", \"size\": " << entry.extraction.size
                     << ", \"sha256\": \"" << entry.extraction.sha256 << "\"}";
        }
        manifest << "\n  ]\n}\n";
        manifest.close();
        if (!manifest) throw std::runtime_error("could not finalize extraction manifest");
        std::filesystem::rename(staging, destination_directory);
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove_all(staging, ignored);
        throw;
    }

    SelectiveExtractionResult result;
    result.destination_directory = destination_directory;
    result.manifest = destination_directory / "rb4pc-manifest.json";
    result.file_count = entries.size();
    for (const auto& entry : entries) result.total_size += entry.extraction.size;
    return result;
}

} // namespace rb4pc::inspector
