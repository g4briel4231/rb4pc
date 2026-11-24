#include "rb4pc/builder/pc_character_catalog.hpp"

#include "rb4pc/inspector/file_inspector.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace rb4pc::builder {
namespace {

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string family_of(const std::string& path) {
    const auto normalized = lower(path);
    for (const auto family : {"charanim", "charclip", "bonemask", "stance", "skel", "char"})
        if (normalized.ends_with(std::string(".") + family + "_pc")) return family;
    return {};
}

std::string hex_prefix(const unsigned char* bytes, std::size_t size) {
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (std::size_t index = 0; index < std::min<std::size_t>(4, size); ++index)
        output << std::setw(2) << static_cast<unsigned int>(bytes[index]);
    return output.str();
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

std::vector<std::filesystem::path> find_headers(const std::filesystem::path& source) {
    std::vector<std::filesystem::path> headers;
    if (std::filesystem::is_regular_file(source)) {
        if (lower(source.extension().string()) != ".hdr")
            throw std::runtime_error("PC character catalog source file is not an HDR");
        headers.push_back(source);
    } else if (std::filesystem::is_directory(source)) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(source))
            if (entry.is_regular_file() && lower(entry.path().extension().string()) == ".hdr")
                headers.push_back(entry.path());
    } else throw std::runtime_error("PC character catalog source does not exist");
    std::sort(headers.begin(), headers.end());
    if (headers.empty()) throw std::runtime_error("PC character catalog found no HDR files");
    return headers;
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

} // namespace

PcCharacterCatalog catalog_pc_characters(const std::filesystem::path& source) {
    const auto headers = find_headers(source);
    PcCharacterCatalog catalog;
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
                if (family.empty()) continue;
                ++catalog.records;
                catalog.bytes += record.size;
                auto& stats = catalog.families[family];
                ++stats.records;
                stats.bytes += record.size;
                if (stats.records == 1) stats.minimum_size = record.size;
                else stats.minimum_size = std::min(stats.minimum_size, record.size);
                stats.maximum_size = std::max(stats.maximum_size, record.size);
                if (record.size == 0) {
                    ++catalog.empty_records;
                    ++stats.first_four_bytes["<empty>"];
                    continue;
                }
                if (index >= report.record_locations.size() || !report.record_locations[index]) {
                    ++catalog.unreadable_records;
                    continue;
                }
                const auto location = *report.record_locations[index];
                if (location.archive_index >= archives.size() || !archives[location.archive_index]) {
                    ++catalog.unreadable_records;
                    continue;
                }
                std::array<unsigned char, 256> bytes{};
                const auto read_size = std::min<std::size_t>(bytes.size(), record.size);
                if (!read_exact(archives[location.archive_index], location.local_offset, bytes.data(), read_size)) {
                    ++catalog.unreadable_records;
                    continue;
                }
                ++stats.first_four_bytes[hex_prefix(bytes.data(), read_size)];
                if (read_size >= 4) ++stats.first_u32_le[std::to_string(read_u32(bytes.data()))];
                for (std::size_t offset = 0; offset + 4 <= std::min<std::size_t>(read_size, 64); offset += 4) {
                    const auto value = read_u32(bytes.data() + offset);
                    auto& field = stats.prefix_u32_fields[offset];
                    ++field.observed;
                    if (field.observed == 1) field.minimum = field.maximum = value;
                    else {
                        field.minimum = std::min(field.minimum, value);
                        field.maximum = std::max(field.maximum, value);
                    }
                    field.distinct_values.insert(value);
                    if (value == 0) ++field.zero_values;
                    if (value <= record.size) ++field.values_within_record;
                    if (value == record.size) ++field.values_equal_record_size;
                    if (value == record.size - offset - 4) ++field.values_equal_remaining_size;
                }
                bool record_has_ascii = false;
                for (std::size_t start = 0; start < read_size;) {
                    if (bytes[start] < 0x20 || bytes[start] > 0x7e) {
                        ++start;
                        continue;
                    }
                    std::size_t end = start;
                    while (end < read_size && bytes[end] >= 0x20 && bytes[end] <= 0x7e) ++end;
                    const auto length = end - start;
                    if (length >= 4) {
                        record_has_ascii = true;
                        ++stats.ascii_strings;
                        stats.ascii_characters += length;
                        stats.maximum_ascii_string_length = std::max(stats.maximum_ascii_string_length, length);
                        ++stats.ascii_length_distribution[length];
                        if (end < read_size && bytes[end] == 0) ++stats.null_terminated_ascii_strings;
                    }
                    start = end;
                }
                if (record_has_ascii) ++stats.records_with_ascii_strings;
            }
        } catch (const std::exception& error) {
            ++catalog.invalid_headers;
            if (catalog.errors.size() < 32)
                catalog.errors.push_back(header.filename().string() + ": " + error.what());
        }
    }
    return catalog;
}

std::string format_json(const PcCharacterCatalog& catalog) {
    std::ostringstream output;
    output << "{\n  \"schemaVersion\": 4,\n"
           << "  \"headers\": {\"candidates\": " << catalog.candidate_headers
           << ", \"valid\": " << catalog.valid_headers
           << ", \"invalid\": " << catalog.invalid_headers << "},\n"
           << "  \"summary\": {\"records\": " << catalog.records
           << ", \"bytes\": " << catalog.bytes
           << ", \"empty\": " << catalog.empty_records
           << ", \"unreadable\": " << catalog.unreadable_records << "},\n"
           << "  \"families\": {";
    bool first_family = true;
    for (const auto& [name, stats] : catalog.families) {
        if (!first_family) output << ',';
        output << "\n    \"" << escape_json(name) << "\": {\"records\": " << stats.records
               << ", \"bytes\": " << stats.bytes << ", \"minimumSize\": " << stats.minimum_size
               << ", \"maximumSize\": " << stats.maximum_size << ", \"firstFourBytes\": {";
        bool first_signature = true;
        for (const auto& [signature, count] : stats.first_four_bytes) {
            if (!first_signature) output << ',';
            output << "\n      \"" << signature << "\": " << count;
            first_signature = false;
        }
        if (!stats.first_four_bytes.empty()) output << "\n    ";
        output << "}, \"firstU32LE\": {";
        bool first_value = true;
        for (const auto& [value, count] : stats.first_u32_le) {
            if (!first_value) output << ',';
            output << "\n      \"" << value << "\": " << count;
            first_value = false;
        }
        if (!stats.first_u32_le.empty()) output << "\n    ";
        output << "}, \"prefixU32Fields\": {";
        bool first_field = true;
        for (const auto& [offset, field] : stats.prefix_u32_fields) {
            if (!first_field) output << ',';
            output << "\n      \"" << offset << "\": {\"observed\": " << field.observed
                   << ", \"distinctValues\": " << field.distinct_values.size()
                   << ", \"zeroValues\": " << field.zero_values
                   << ", \"valuesWithinRecord\": " << field.values_within_record
                   << ", \"valuesEqualRecordSize\": " << field.values_equal_record_size
                   << ", \"valuesEqualRemainingSize\": " << field.values_equal_remaining_size
                   << ", \"minimum\": " << field.minimum
                   << ", \"maximum\": " << field.maximum << '}';
            first_field = false;
        }
        if (!stats.prefix_u32_fields.empty()) output << "\n    ";
        output << "}, \"candidateFields\": {\"constant\": [";
        bool first_candidate = true;
        for (const auto& [offset, field] : stats.prefix_u32_fields) {
            if (field.observed != stats.records || field.distinct_values.size() != 1) continue;
            if (!first_candidate) output << ", ";
            output << offset;
            first_candidate = false;
        }
        output << "], \"recordSize\": {";
        first_candidate = true;
        for (const auto& [offset, field] : stats.prefix_u32_fields) {
            if (field.values_equal_record_size == 0) continue;
            if (!first_candidate) output << ", ";
            output << '"' << offset << "\": " << field.values_equal_record_size;
            first_candidate = false;
        }
        output << "}, \"remainingSize\": {";
        first_candidate = true;
        for (const auto& [offset, field] : stats.prefix_u32_fields) {
            if (field.values_equal_remaining_size == 0) continue;
            if (!first_candidate) output << ", ";
            output << '"' << offset << "\": " << field.values_equal_remaining_size;
            first_candidate = false;
        }
        output << "}}, \"asciiSignals\": {\"recordsWithStrings\": "
               << stats.records_with_ascii_strings << ", \"strings\": " << stats.ascii_strings
               << ", \"nullTerminated\": " << stats.null_terminated_ascii_strings
               << ", \"characters\": " << stats.ascii_characters
               << ", \"maximumLength\": " << stats.maximum_ascii_string_length
               << ", \"lengthDistribution\": {";
        bool first_length = true;
        for (const auto& [length, count] : stats.ascii_length_distribution) {
            if (!first_length) output << ", ";
            output << '"' << length << "\": " << count;
            first_length = false;
        }
        output << "}}}";
        first_family = false;
    }
    if (!catalog.families.empty()) output << '\n';
    output << "  },\n  \"errors\": [";
    for (std::size_t index = 0; index < catalog.errors.size(); ++index) {
        if (index != 0) output << ',';
        output << "\n    \"" << escape_json(catalog.errors[index]) << '"';
    }
    if (!catalog.errors.empty()) output << "\n  ";
    output << "]\n}\n";
    return output.str();
}

} // namespace rb4pc::builder
