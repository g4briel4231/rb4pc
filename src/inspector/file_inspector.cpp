#include "rb4pc/inspector/file_inspector.hpp"
#include "rb4pc/inspector/sha256.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace rb4pc::inspector {
namespace {

struct RecordSummary {
    std::uint64_t declared_archive_bytes = 0;
    std::size_t offsets_in_range = 0;
    std::size_t spans_in_range = 0;
    std::size_t zero_size_records = 0;
    std::size_t marker_records = 0;
    std::size_t sentinel_value0_records = 0;
};

RecordSummary summarize_records(const HeaderSetReport& report) {
    RecordSummary summary;
    for (const auto& archive : report.metadata.archives) summary.declared_archive_bytes += archive.declared_size;
    for (const auto& record : report.metadata.file_records) {
        if (record.global_offset <= summary.declared_archive_bytes) ++summary.offsets_in_range;
        if (record.global_offset <= summary.declared_archive_bytes &&
            record.size <= summary.declared_archive_bytes - record.global_offset) {
            ++summary.spans_in_range;
        }
        if (record.size == 0) ++summary.zero_size_records;
        if (record.path.starts_with("__marker__") || record.path == "__split_ark__") ++summary.marker_records;
        if (record.previous_hash_index == std::numeric_limits<std::uint32_t>::max()) {
            ++summary.sentinel_value0_records;
        }
    }
    return summary;
}

std::string platform_hint(const std::filesystem::path& path) {
    auto name = path.stem().string();
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    if (name.ends_with("_ps4")) return "ps4";
    if (name.ends_with("_xb1")) return "xb1";
    if (name.ends_with("_pc")) return "pc";
    return "unknown";
}

std::string escape_json(const std::string& value) {
    std::ostringstream output;
    for (const unsigned char character : value) {
        switch (character) {
        case '"': output << "\\\""; break;
        case '\\': output << "\\\\"; break;
        case '\b': output << "\\b"; break;
        case '\f': output << "\\f"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (character < 0x20) {
                output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                       << static_cast<unsigned int>(character) << std::dec;
            } else {
                output << static_cast<char>(character);
            }
        }
    }
    return output.str();
}

std::string preview_hex(const FileReport& report) {
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (std::size_t index = 0; index < report.preview_size; ++index) {
        output << std::setw(2) << std::to_integer<unsigned int>(report.preview[index]);
    }
    return output.str();
}

} // namespace

FileReport inspect_file(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("file does not exist: " + path.string());
    }
    if (!std::filesystem::is_regular_file(path)) {
        throw std::runtime_error("path is not a regular file: " + path.string());
    }

    FileReport report;
    report.path = path;
    report.size = std::filesystem::file_size(path);

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("could not open file: " + path.string());
    }

    input.read(reinterpret_cast<char*>(report.preview.data()),
               static_cast<std::streamsize>(report.preview.size()));
    report.preview_size = static_cast<std::size_t>(input.gcount());
    report.sha256 = sha256_file(path);
    return report;
}

HeaderSetReport inspect_header_set(const std::filesystem::path& path) {
    HeaderSetReport report;
    report.platform_hint = platform_hint(path);
    report.header = inspect_file(path);
    report.analysis = analyze_binary(path);
    report.metadata = decode_hdr_prefix(path);

    const auto directory = path.has_parent_path() ? path.parent_path() : std::filesystem::path{"."};
    for (const auto& declared : report.metadata.archives) {
        const auto archive_path = directory / std::filesystem::path(declared.name);
        const auto exists = std::filesystem::is_regular_file(archive_path);
        const auto actual_size = exists ? std::filesystem::file_size(archive_path) : 0;
        report.archives.push_back({archive_path, declared.declared_size, actual_size, exists,
                                   exists && actual_size == declared.declared_size});
    }
    return report;
}

std::string format_report(const FileReport& report) {
    std::ostringstream output;
    output << "Path: " << report.path.string() << '\n';
    output << "Size: " << report.size << " bytes\n";
    output << "SHA-256: " << report.sha256 << '\n';
    output << "First " << report.preview_size << " bytes:";

    if (report.preview_size == 0) {
        output << " <empty>\n";
        return output.str();
    }

    output << std::hex << std::setfill('0');
    for (std::size_t index = 0; index < report.preview_size; ++index) {
        if (index % 16 == 0) {
            output << '\n';
        } else {
            output << ' ';
        }
        output << std::setw(2) << std::to_integer<unsigned int>(report.preview[index]);
    }
    output << '\n';
    return output.str();
}

std::string format_report(const HeaderSetReport& report) {
    std::ostringstream output;
    const auto record_summary = summarize_records(report);
    output << format_report(report.header);
    output << "Platform hint: " << report.platform_hint << " (filename only)\n";
    output << std::fixed << std::setprecision(6);
    output << "Shannon entropy: " << report.analysis.entropy << " bits/byte\n";
    output << "Zero bytes: " << report.analysis.zero_bytes << '\n';
    output << "Analysis blocks: " << report.analysis.blocks.size() << " x up to "
           << report.analysis.block_size << " bytes\n";
    output << "Repeated full blocks: " << report.analysis.repeated_full_blocks << '\n';
    output << "Decoded HDR version: " << report.metadata.version << '\n';
    output << "Decoded unknown value at 0x04: " << report.metadata.unknown_04 << '\n';
    output << "Decoded 16-byte value at 0x08: "
           << hex_string(report.metadata.unknown_08.data(), report.metadata.unknown_08.size()) << '\n';
    output << "Declared ARK volumes: " << report.archives.size() << '\n';
    for (std::size_t index = 0; index < report.archives.size(); ++index) {
        const auto& archive = report.archives[index];
        output << "  [" << index << "] " << archive.path.filename().string()
               << " — declared " << archive.declared_size << " bytes";
        if (!archive.exists) output << " — missing";
        else if (!archive.size_matches) output << " — actual " << archive.actual_size << " bytes — mismatch";
        else output << " — verified";
        output << '\n';
    }
    output << "Decoded file records: " << report.metadata.file_records.size() << '\n';
    output << "Declared concatenated ARK bytes: " << record_summary.declared_archive_bytes << '\n';
    output << "Record offsets in declared range: " << record_summary.offsets_in_range << '\n';
    output << "Record offset+size spans in range: " << record_summary.spans_in_range << '\n';
    output << "Zero-size records: " << record_summary.zero_size_records << '\n';
    output << "Marker records: " << record_summary.marker_records << '\n';
    const auto sample_count = std::min<std::size_t>(report.metadata.file_records.size(), 5);
    for (std::size_t index = 0; index < sample_count; ++index) {
        const auto& record = report.metadata.file_records[index];
        output << "  record[" << index << "]: globalOffset=" << record.global_offset
               << ", path=" << record.path
               << ", previousHashIndex=" << record.previous_hash_index
               << ", size=" << record.size << '\n';
    }
    return output.str();
}

std::string format_json(const HeaderSetReport& report) {
    std::ostringstream output;
    const auto record_summary = summarize_records(report);
    output << "{\n";
    output << "  \"schemaVersion\": 4,\n";
    output << "  \"platformHint\": \"" << escape_json(report.platform_hint) << "\",\n";
    output << "  \"platformHintSource\": \"filename\",\n";
    output << "  \"header\": {\n";
    output << "    \"name\": \"" << escape_json(report.header.path.filename().string()) << "\",\n";
    output << "    \"size\": " << report.header.size << ",\n";
    output << "    \"sha256\": \"" << report.header.sha256 << "\",\n";
    output << "    \"firstBytesHex\": \"" << preview_hex(report.header) << "\"\n";
    output << "  },\n";
    output << "  \"hdr\": {\n";
    output << "    \"rawSeed\": " << report.metadata.raw_seed << ",\n";
    output << "    \"version\": " << report.metadata.version << ",\n";
    output << "    \"unknown04\": " << report.metadata.unknown_04 << ",\n";
    output << "    \"unknown08Hex\": \""
           << hex_string(report.metadata.unknown_08.data(), report.metadata.unknown_08.size()) << "\",\n";
    output << "    \"fileRecordCount\": " << report.metadata.file_records.size() << ",\n";
    output << "    \"hashBucketCount\": " << report.metadata.hash_bucket_heads.size() << ",\n";
    output << "    \"hashIndexValidated\": "
           << (report.metadata.hash_index_validated ? "true" : "false") << ",\n";
    output << "    \"decodedPayloadSize\": " << report.metadata.decoded_payload_size << "\n";
    output << "  },\n";
    output << "  \"fileRecordSamples\": [";
    const auto record_sample_count = std::min<std::size_t>(report.metadata.file_records.size(), 5);
    for (std::size_t index = 0; index < record_sample_count; ++index) {
        const auto& record = report.metadata.file_records[index];
        output << (index == 0 ? "\n" : ",\n");
        output << "    {\"globalOffset\": " << record.global_offset
               << ", \"path\": \"" << escape_json(record.path)
               << "\", \"previousHashIndex\": " << record.previous_hash_index
               << ", \"size\": " << record.size << "}";
    }
    if (record_sample_count != 0) output << '\n';
    output << "  ],\n";
    output << "  \"fileRecordAnalysis\": {\n";
    output << "    \"declaredArchiveBytes\": " << record_summary.declared_archive_bytes << ",\n";
    output << "    \"offsetsInRange\": " << record_summary.offsets_in_range << ",\n";
    output << "    \"spansInRange\": " << record_summary.spans_in_range << ",\n";
    output << "    \"zeroSizeRecords\": " << record_summary.zero_size_records << ",\n";
    output << "    \"markerRecords\": " << record_summary.marker_records << ",\n";
    output << "    \"sentinelPreviousHashIndexRecords\": " << record_summary.sentinel_value0_records << "\n";
    output << "  },\n";
    output << std::fixed << std::setprecision(6);
    output << "  \"analysis\": {\n";
    output << "    \"method\": \"shannon-byte-frequency\",\n";
    output << "    \"entropyBitsPerByte\": " << report.analysis.entropy << ",\n";
    output << "    \"zeroBytes\": " << report.analysis.zero_bytes << ",\n";
    output << "    \"blockSize\": " << report.analysis.block_size << ",\n";
    output << "    \"repeatedFullBlocks\": " << report.analysis.repeated_full_blocks << ",\n";
    output << "    \"blocks\": [";
    for (std::size_t index = 0; index < report.analysis.blocks.size(); ++index) {
        const auto& block = report.analysis.blocks[index];
        output << (index == 0 ? "\n" : ",\n");
        output << "      {\"offset\": " << block.offset << ", \"size\": " << block.size
               << ", \"entropyBitsPerByte\": " << block.entropy << "}";
    }
    if (!report.analysis.blocks.empty()) output << '\n';
    output << "    ]\n";
    output << "  },\n";
    output << "  \"archives\": [";
    for (std::size_t index = 0; index < report.archives.size(); ++index) {
        const auto& archive = report.archives[index];
        output << (index == 0 ? "\n" : ",\n");
        output << "    {\"index\": " << index
               << ", \"name\": \"" << escape_json(archive.path.filename().string())
               << "\", \"declaredSize\": " << archive.declared_size
               << ", \"exists\": " << (archive.exists ? "true" : "false")
               << ", \"actualSize\": " << archive.actual_size
               << ", \"sizeMatches\": " << (archive.size_matches ? "true" : "false")
               << ", \"tags\": [";
        const auto& tags = report.metadata.archives[index].tags;
        for (std::size_t tag_index = 0; tag_index < tags.size(); ++tag_index) {
            if (tag_index != 0) output << ", ";
            output << "\"" << escape_json(tags[tag_index]) << "\"";
        }
        output << "]}";
    }
    if (!report.archives.empty()) output << '\n';
    output << "  ]\n";
    output << "}\n";
    return output.str();
}

} // namespace rb4pc::inspector

