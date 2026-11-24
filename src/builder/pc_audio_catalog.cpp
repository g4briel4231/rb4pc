#include "rb4pc/builder/pc_audio_catalog.hpp"

#include "rb4pc/inspector/file_inspector.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace rb4pc::builder {
namespace {

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

enum class AudioFamily { none, mogg, wave, bank, opaque };

AudioFamily family_of(const std::string& path) {
    const auto normalized = lower(path);
    if (normalized.ends_with(".mogg")) return AudioFamily::mogg;
    if (normalized.ends_with(".wav")) return AudioFamily::wave;
    if (normalized.ends_with(".bank") || normalized.ends_with(".fev")) return AudioFamily::bank;
    if (normalized.ends_with(".noises_pc")) return AudioFamily::opaque;
    return AudioFamily::none;
}

std::uint32_t read_u32(const unsigned char* bytes) {
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8) |
           (static_cast<std::uint32_t>(bytes[2]) << 16) |
           (static_cast<std::uint32_t>(bytes[3]) << 24);
}

bool read_exact(std::ifstream& input, std::uint64_t offset, unsigned char* bytes, std::size_t size) {
    input.clear();
    input.seekg(static_cast<std::streamoff>(offset));
    if (!input) return false;
    input.read(reinterpret_cast<char*>(bytes), static_cast<std::streamsize>(size));
    return static_cast<std::size_t>(input.gcount()) == size;
}

std::string fourcc(const unsigned char* bytes) {
    return {reinterpret_cast<const char*>(bytes), 4};
}

std::string escape_json(std::string_view value) {
    std::string result;
    for (const char c : value) {
        if (c == '\\' || c == '"') result.push_back('\\');
        if (c == '\n') result += "\\n";
        else if (c == '\r') result += "\\r";
        else if (c == '\t') result += "\\t";
        else if (static_cast<unsigned char>(c) >= 0x20) result.push_back(c);
    }
    return result;
}

void write_map(std::ostringstream& output, const std::map<std::string, std::size_t>& values) {
    output << '{';
    bool first = true;
    for (const auto& [key, count] : values) {
        if (!first) output << ',';
        output << "\n      \"" << escape_json(key) << "\": " << count;
        first = false;
    }
    if (!values.empty()) output << "\n    ";
    output << '}';
}

std::vector<std::filesystem::path> find_headers(const std::filesystem::path& source) {
    std::vector<std::filesystem::path> headers;
    if (std::filesystem::is_regular_file(source)) {
        if (lower(source.extension().string()) != ".hdr")
            throw std::runtime_error("PC audio catalog source file is not an HDR");
        headers.push_back(source);
    } else if (std::filesystem::is_directory(source)) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(source))
            if (entry.is_regular_file() && lower(entry.path().extension().string()) == ".hdr")
                headers.push_back(entry.path());
    } else throw std::runtime_error("PC audio catalog source does not exist");
    std::sort(headers.begin(), headers.end());
    if (headers.empty()) throw std::runtime_error("PC audio catalog found no HDR files");
    return headers;
}

} // namespace

PcAudioCatalog catalog_pc_audio(const std::filesystem::path& source) {
    const auto headers = find_headers(source);
    PcAudioCatalog catalog;
    catalog.candidate_headers = headers.size();
    for (const auto& header : headers) {
        try {
            const auto report = inspector::inspect_header_set(header);
            ++catalog.valid_headers;
            std::vector<std::ifstream> archives;
            archives.reserve(report.archives.size());
            for (const auto& archive : report.archives) archives.emplace_back(archive.path, std::ios::binary);

            for (std::size_t index = 0; index < report.metadata.file_records.size(); ++index) {
                const auto& record = report.metadata.file_records[index];
                const auto family = family_of(record.path);
                if (family == AudioFamily::none) continue;
                ++catalog.audio_records;
                catalog.audio_bytes += record.size;
                if (family == AudioFamily::mogg) ++catalog.mogg_records;
                else if (family == AudioFamily::wave) ++catalog.wave_records;
                else if (family == AudioFamily::bank) ++catalog.bank_records;
                else ++catalog.opaque_records;

                if (index >= report.record_locations.size() || !report.record_locations[index]) {
                    ++catalog.unreadable_records;
                    ++catalog.invalid_reasons["unresolved-record"];
                    continue;
                }
                const auto location = *report.record_locations[index];
                if (location.archive_index >= archives.size() || !archives[location.archive_index]) {
                    ++catalog.unreadable_records;
                    ++catalog.invalid_reasons["missing-archive"];
                    continue;
                }
                auto& input = archives[location.archive_index];
                if (family == AudioFamily::mogg) {
                    if (record.size < 12) {
                        ++catalog.invalid_reasons["short-mogg"];
                        continue;
                    }
                    std::array<unsigned char, 12> header_bytes{};
                    if (!read_exact(input, location.local_offset, header_bytes.data(), header_bytes.size())) {
                        ++catalog.unreadable_records;
                        ++catalog.invalid_reasons["unreadable-mogg-header"];
                        continue;
                    }
                    const auto version = read_u32(header_bytes.data());
                    const auto ogg_offset = read_u32(header_bytes.data() + 4);
                    if (version == 10) {
                        ++catalog.mogg_versions["10"];
                        ++catalog.mogg_version_10;
                    }
                    else {
                        ++catalog.mogg_versions["unrecognized"];
                        ++catalog.mogg_unsupported_or_protected;
                        ++catalog.invalid_reasons["unsupported-or-protected-mogg-header"];
                        continue;
                    }
                    if (ogg_offset > record.size - 4) {
                        ++catalog.invalid_reasons["mogg-ogg-offset-out-of-range"];
                        continue;
                    }
                    std::array<unsigned char, 4> signature{};
                    if (!read_exact(input, location.local_offset + ogg_offset, signature.data(), signature.size())) {
                        ++catalog.unreadable_records;
                        ++catalog.invalid_reasons["unreadable-mogg-ogg-signature"];
                    } else if (fourcc(signature.data()) == "OggS") ++catalog.mogg_embedded_ogg;
                    else ++catalog.invalid_reasons["missing-embedded-ogg"];
                } else if (family == AudioFamily::wave || family == AudioFamily::bank) {
                    if (record.size < 12) {
                        ++catalog.invalid_reasons["short-riff"];
                        continue;
                    }
                    std::array<unsigned char, 12> bytes{};
                    if (!read_exact(input, location.local_offset, bytes.data(), bytes.size())) {
                        ++catalog.unreadable_records;
                        ++catalog.invalid_reasons["unreadable-riff-header"];
                        continue;
                    }
                    if (fourcc(bytes.data()) != "RIFF") ++catalog.invalid_reasons["missing-riff"];
                    else if (fourcc(bytes.data() + 8) == "WAVE") ++catalog.riff_wave;
                    else if (fourcc(bytes.data() + 8) == "FEV ") ++catalog.riff_fev;
                    else ++catalog.invalid_reasons["unexpected-riff-form"];
                }
            }
        } catch (const std::exception& error) {
            ++catalog.invalid_headers;
            if (catalog.errors.size() < 32)
                catalog.errors.push_back(header.filename().string() + ": " + error.what());
        }
    }
    return catalog;
}

std::string format_json(const PcAudioCatalog& catalog) {
    std::ostringstream output;
    output << "{\n  \"schemaVersion\": 1,\n"
           << "  \"headers\": {\"candidates\": " << catalog.candidate_headers
           << ", \"valid\": " << catalog.valid_headers
           << ", \"invalid\": " << catalog.invalid_headers << "},\n"
           << "  \"audio\": {\"records\": " << catalog.audio_records
           << ", \"bytes\": " << catalog.audio_bytes
           << ", \"opaque\": " << catalog.opaque_records
           << ", \"unreadable\": " << catalog.unreadable_records << "},\n"
           << "  \"mogg\": {\"records\": " << catalog.mogg_records
           << ", \"version10\": " << catalog.mogg_version_10
           << ", \"embeddedOgg\": " << catalog.mogg_embedded_ogg
           << ", \"unsupportedOrProtected\": " << catalog.mogg_unsupported_or_protected
           << ", \"versions\": ";
    write_map(output, catalog.mogg_versions);
    output << "},\n  \"riff\": {\"waveRecords\": " << catalog.wave_records
           << ", \"waveForms\": " << catalog.riff_wave
           << ", \"bankRecords\": " << catalog.bank_records
           << ", \"fevForms\": " << catalog.riff_fev << "},\n"
           << "  \"invalidReasons\": ";
    write_map(output, catalog.invalid_reasons);
    output << ",\n  \"errors\": [";
    for (std::size_t index = 0; index < catalog.errors.size(); ++index) {
        if (index != 0) output << ',';
        output << "\n    \"" << escape_json(catalog.errors[index]) << '"';
    }
    if (!catalog.errors.empty()) output << "\n  ";
    output << "]\n}\n";
    return output.str();
}

} // namespace rb4pc::builder
