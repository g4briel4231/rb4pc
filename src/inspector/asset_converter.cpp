#include "rb4pc/inspector/asset_converter.hpp"
#include "rb4pc/inspector/archive_layout.hpp"
#include "rb4pc/inspector/file_inspector.hpp"
#include "rb4pc/inspector/riff_parser.hpp"
#include "rb4pc/inspector/sha256.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <stdexcept>
#include <sstream>

namespace rb4pc::inspector {
namespace {
void copy_range(std::ifstream& input, std::uint64_t offset, std::uint64_t size,
                const std::filesystem::path& output_path) {
    input.clear();
    input.seekg(static_cast<std::streamoff>(offset));
    if (!input) throw std::runtime_error("could not seek within ARK volume");
    std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("could not create conversion output");
    std::array<char, 64 * 1024> buffer{};
    while (size != 0) {
        const auto chunk = static_cast<std::size_t>(std::min<std::uint64_t>(size, buffer.size()));
        input.read(buffer.data(), static_cast<std::streamsize>(chunk));
        if (static_cast<std::size_t>(input.gcount()) != chunk) throw std::runtime_error("unexpected end of ARK during conversion");
        output.write(buffer.data(), static_cast<std::streamsize>(chunk));
        if (!output) throw std::runtime_error("could not write conversion output");
        size -= chunk;
    }
}
} // namespace

RiffConversionResult convert_riff_record(const HeaderSetReport& report, std::string_view internal_path,
                                         const std::filesystem::path& destination) {
    if (destination.empty() || std::filesystem::exists(destination)) throw std::runtime_error("RIFF destination is empty or exists");
    const auto parent = destination.has_parent_path() ? destination.parent_path() : std::filesystem::path{"."};
    if (!std::filesystem::is_directory(parent)) throw std::runtime_error("conversion directory does not exist");
    const auto index = find_file_record(report.metadata, internal_path);
    if (!index || *index >= report.record_locations.size() || !report.record_locations[*index]) throw std::runtime_error("RIFF record cannot be resolved");
    const auto location = *report.record_locations[*index];
    if (location.archive_index >= report.archives.size() || !report.archives[location.archive_index].exists) throw std::runtime_error("RIFF ARK volume is missing");
    const auto& record = report.metadata.file_records[*index];
    std::ifstream input(report.archives[location.archive_index].path, std::ios::binary);
    if (!input) throw std::runtime_error("could not open RIFF ARK volume");
    const auto riff = analyze_riff(input, location.local_offset, record.size);
    if (!riff.structure_valid) throw std::runtime_error("record is not a structurally valid RIFF");

    auto logical_temp = destination; logical_temp += ".rb4pc-part";
    auto sidecar = destination; sidecar += ".rb4pc-trailing.bin";
    auto sidecar_temp = sidecar; sidecar_temp += ".rb4pc-part";
    auto manifest = destination; manifest += ".rb4pc-conversion.json";
    auto manifest_temp = manifest; manifest_temp += ".rb4pc-part";
    if (std::filesystem::exists(logical_temp) || std::filesystem::exists(sidecar) ||
        std::filesystem::exists(sidecar_temp) || std::filesystem::exists(manifest) ||
        std::filesystem::exists(manifest_temp)) throw std::runtime_error("conversion temporary, sidecar, or manifest path exists");
    RiffConversionResult result;
    result.destination = destination;
    result.record_size = record.size;
    result.logical_size = riff.declared_file_size;
    result.form_type = riff.form_type;
    result.manifest = manifest;
    try {
        copy_range(input, location.local_offset, riff.declared_file_size, logical_temp);
        if (riff.trailing_bytes_size != 0) {
            copy_range(input, location.local_offset + riff.declared_file_size, riff.trailing_bytes_size, sidecar_temp);
            std::filesystem::rename(sidecar_temp, sidecar);
            result.trailing_sidecar = sidecar;
            result.trailing_sha256 = sha256_file(sidecar);
        }
        std::filesystem::rename(logical_temp, destination);
        std::ifstream verification(destination, std::ios::binary);
        const auto verified = analyze_riff(verification, 0, riff.declared_file_size);
        if (!verified.structure_valid || !verified.size_matches_record || verified.form_type != riff.form_type) {
            throw std::runtime_error("converted RIFF failed structural verification");
        }
        result.sha256 = sha256_file(destination);
        std::ofstream manifest_output(manifest_temp, std::ios::binary | std::ios::trunc);
        if (!manifest_output) throw std::runtime_error("could not create conversion manifest");
        manifest_output << "{\n  \"schemaVersion\": 1,\n"
                        << "  \"internalPath\": \"" << internal_path << "\",\n"
                        << "  \"formType\": \"" << result.form_type << "\",\n"
                        << "  \"recordSize\": " << result.record_size << ",\n"
                        << "  \"logicalSize\": " << result.logical_size << ",\n"
                        << "  \"sha256\": \"" << result.sha256 << "\"";
        if (result.trailing_sidecar) {
            manifest_output << ",\n  \"trailingSidecar\": \"" << sidecar.filename().string()
                            << "\",\n  \"trailingSha256\": \"" << *result.trailing_sha256 << "\"";
        }
        manifest_output << "\n}\n";
        manifest_output.close();
        if (!manifest_output) throw std::runtime_error("could not finalize conversion manifest");
        std::filesystem::rename(manifest_temp, manifest);
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(logical_temp, ignored); std::filesystem::remove(sidecar_temp, ignored);
        if (result.trailing_sidecar) std::filesystem::remove(sidecar, ignored);
        std::filesystem::remove(manifest_temp, ignored); std::filesystem::remove(manifest, ignored);
        std::filesystem::remove(destination, ignored);
        throw;
    }
    return result;
}
} // namespace rb4pc::inspector
