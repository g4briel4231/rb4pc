#include "rb4pc/builder/pc_metadata_catalog.hpp"
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
    const auto name = lower(path);
    for (const auto family : {"moggsong_dta_pc", "songdta_pc", "csv_pc", "rbsong", "dta"}) {
        if (name.ends_with(std::string(".") + family)) return family;
    }
    return {};
}

std::vector<std::filesystem::path> find_headers(const std::filesystem::path& source) {
    std::vector<std::filesystem::path> headers;
    if (std::filesystem::is_regular_file(source)) {
        if (lower(source.extension().string()) != ".hdr")
            throw std::runtime_error("PC metadata catalog source file is not an HDR");
        headers.push_back(source);
    } else if (std::filesystem::is_directory(source)) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(source))
            if (entry.is_regular_file() && lower(entry.path().extension().string()) == ".hdr")
                headers.push_back(entry.path());
    } else throw std::runtime_error("PC metadata catalog source does not exist");
    std::sort(headers.begin(), headers.end());
    if (headers.empty()) throw std::runtime_error("PC metadata catalog found no HDR files");
    return headers;
}

bool read_exact(std::ifstream& input, std::uint64_t offset, unsigned char* bytes, std::size_t size) {
    input.clear();
    input.seekg(static_cast<std::streamoff>(offset));
    if (!input) return false;
    input.read(reinterpret_cast<char*>(bytes), static_cast<std::streamsize>(size));
    return static_cast<std::size_t>(input.gcount()) == size;
}

std::string hex_prefix(const unsigned char* bytes, std::size_t size) {
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (std::size_t index = 0; index < std::min<std::size_t>(16, size); ++index)
        output << std::setw(2) << static_cast<unsigned int>(bytes[index]);
    return output.str();
}

std::uint32_t read_u32(const unsigned char* bytes) {
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8) |
           (static_cast<std::uint32_t>(bytes[2]) << 16) |
           (static_cast<std::uint32_t>(bytes[3]) << 24);
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

std::size_t PcMetadataCatalog::profiled_fields() const {
    std::size_t total = 0;
    for (const auto& [name, stats] : families) {
        (void)name;
        total += stats.prefix_u32_fields.size();
    }
    return total;
}

std::size_t PcMetadataCatalog::classified_fields() const {
    std::size_t total = 0;
    for (const auto& [name, stats] : families) {
        (void)name;
        total += stats.cross_header_constant_fields + stats.header_variant_constant_fields +
                 stats.record_variable_fields + stats.incomplete_header_coverage_fields;
    }
    return total;
}

bool PcMetadataCatalog::evidence_classification_complete() const {
    return profiled_fields() == classified_fields();
}

PcMetadataCatalog catalog_pc_metadata(const std::filesystem::path& source) {
    const auto headers = find_headers(source);
    PcMetadataCatalog catalog;
    catalog.candidate_headers = headers.size();
    for (const auto& header : headers) {
        try {
            const auto report = inspector::inspect_header_set(header);
            ++catalog.valid_headers;
            std::vector<std::ifstream> archives;
            for (const auto& archive : report.archives) archives.emplace_back(archive.path, std::ios::binary);
            std::map<std::string, std::size_t> header_family_records;
            std::map<std::string, std::map<std::size_t, std::set<std::uint32_t>>> header_field_values;
            for (std::size_t index = 0; index < report.metadata.file_records.size(); ++index) {
                const auto& record = report.metadata.file_records[index];
                const auto family = family_of(record.path);
                if (family.empty()) continue;
                ++header_family_records[family];
                ++catalog.records;
                catalog.bytes += record.size;
                auto& stats = catalog.families[family];
                ++stats.records;
                stats.bytes += record.size;
                if (stats.records == 1) stats.minimum_size = record.size;
                else stats.minimum_size = std::min(stats.minimum_size, record.size);
                stats.maximum_size = std::max(stats.maximum_size, record.size);
                if (record.size == 0) { ++stats.empty_records; continue; }
                if (index >= report.record_locations.size() || !report.record_locations[index]) {
                    ++stats.unreadable_records; ++catalog.unreadable_records; continue;
                }
                const auto location = *report.record_locations[index];
                if (location.archive_index >= archives.size() || !archives[location.archive_index]) {
                    ++stats.unreadable_records; ++catalog.unreadable_records; continue;
                }
                std::array<unsigned char, 256> bytes{};
                const auto size = std::min<std::size_t>(bytes.size(), record.size);
                if (!read_exact(archives[location.archive_index], location.local_offset, bytes.data(), size)) {
                    ++stats.unreadable_records; ++catalog.unreadable_records; continue;
                }
                ++stats.first_sixteen_bytes[hex_prefix(bytes.data(), size)];
                if (size >= 4) ++stats.first_u32_le[std::to_string(read_u32(bytes.data()))];
                for (std::size_t offset = 0; offset + 4 <= std::min<std::size_t>(size, 64); offset += 4) {
                    const auto value = read_u32(bytes.data() + offset);
                    header_field_values[family][offset].insert(value);
                    auto& field = stats.prefix_u32_fields[offset];
                    ++field.observed;
                    if (field.observed == 1) field.minimum = field.maximum = value;
                    else {
                        field.minimum = std::min(field.minimum, value);
                        field.maximum = std::max(field.maximum, value);
                    }
                    field.distinct_values.insert(value);
                    if (value == 0) ++field.zero_values;
                    else ++field.nonzero_values;
                    if (value > 0 && value < record.size) {
                        ++field.values_within_record_bounds;
                        if (value % 4 == 0) ++field.aligned_values_within_record_bounds;
                        if (value < 64) ++field.values_within_profile_prefix;
                        else ++field.values_within_payload_region;
                    }
                    if (value == record.size) ++field.values_equal_record_size;
                    if (value == record.size - offset - 4) ++field.values_equal_remaining_size;
                }
                bool has_ascii = false;
                for (std::size_t start = 0; start < size;) {
                    if (bytes[start] < 0x20 || bytes[start] > 0x7e) { ++start; continue; }
                    std::size_t end = start;
                    while (end < size && bytes[end] >= 0x20 && bytes[end] <= 0x7e) ++end;
                    const auto length = end - start;
                    if (length >= 4) {
                        has_ascii = true; ++stats.ascii_runs; ++stats.ascii_length_distribution[length];
                        stats.maximum_ascii_length = std::max(stats.maximum_ascii_length, length);
                        if (end < size && bytes[end] == 0) ++stats.null_terminated_ascii_runs;
                    }
                    start = end;
                }
                if (has_ascii) ++stats.records_with_ascii;
            }
            for (const auto& [family, records] : header_family_records) {
                auto& stats = catalog.families[family];
                ++stats.headers_observed;
                if (stats.headers_observed == 1) stats.minimum_records_per_header = records;
                else stats.minimum_records_per_header = std::min(stats.minimum_records_per_header, records);
                stats.maximum_records_per_header = std::max(stats.maximum_records_per_header, records);
            }
            for (const auto& [family, fields] : header_field_values) {
                auto& stats = catalog.families[family];
                for (const auto& [offset, values] : fields) {
                    auto& field = stats.prefix_u32_fields[offset];
                    ++field.headers_observed;
                    if (values.size() == 1) {
                        ++field.headers_constant;
                        field.distinct_header_constants.insert(*values.begin());
                    }
                }
            }
        } catch (const std::exception& error) {
            ++catalog.invalid_headers;
            if (catalog.errors.size() < 32) catalog.errors.push_back(header.filename().string() + ": " + error.what());
        }
    }
    for (auto& [family, stats] : catalog.families) {
        (void)family;
        for (const auto& [offset, field] : stats.prefix_u32_fields) {
            (void)offset;
            if (field.headers_observed != stats.headers_observed) {
                ++stats.incomplete_header_coverage_fields;
            } else if (field.headers_constant != field.headers_observed) {
                ++stats.record_variable_fields;
            } else if (field.distinct_header_constants.size() == 1) {
                ++stats.cross_header_constant_fields;
            } else {
                ++stats.header_variant_constant_fields;
            }
        }
    }
    return catalog;
}

std::string format_json(const PcMetadataCatalog& catalog) {
    std::ostringstream output;
    output << "{\n  \"schemaVersion\": 8,\n  \"headers\": {\"candidates\": "
           << catalog.candidate_headers << ", \"valid\": " << catalog.valid_headers
           << ", \"invalid\": " << catalog.invalid_headers << "},\n  \"summary\": {\"records\": "
           << catalog.records << ", \"bytes\": " << catalog.bytes
           << ", \"unreadable\": " << catalog.unreadable_records
           << ", \"profiledFields\": " << catalog.profiled_fields()
           << ", \"classifiedFields\": " << catalog.classified_fields()
           << ", \"evidenceClassificationComplete\": "
           << (catalog.evidence_classification_complete() ? "true" : "false")
           << "},\n  \"families\": {";
    bool first_family = true;
    for (const auto& [name, stats] : catalog.families) {
        if (!first_family) output << ',';
        output << "\n    \"" << escape_json(name) << "\": {\"records\": " << stats.records
               << ", \"bytes\": " << stats.bytes
               << ", \"headerCoverage\": {\"observed\": " << stats.headers_observed
               << ", \"minimumRecords\": " << stats.minimum_records_per_header
               << ", \"maximumRecords\": " << stats.maximum_records_per_header << "}"
               << ", \"fieldEvidence\": {\"crossHeaderConstant\": " << stats.cross_header_constant_fields
               << ", \"headerVariantConstant\": " << stats.header_variant_constant_fields
               << ", \"recordVariable\": " << stats.record_variable_fields
               << ", \"incompleteHeaderCoverage\": " << stats.incomplete_header_coverage_fields << "}"
               << ", \"minimumSize\": " << stats.minimum_size
               << ", \"maximumSize\": " << stats.maximum_size << ", \"empty\": " << stats.empty_records
               << ", \"unreadable\": " << stats.unreadable_records
               << ", \"recordsWithAscii\": " << stats.records_with_ascii
               << ", \"asciiRuns\": " << stats.ascii_runs
               << ", \"nullTerminatedAsciiRuns\": " << stats.null_terminated_ascii_runs
               << ", \"maximumAsciiLength\": " << stats.maximum_ascii_length
               << ", \"firstSixteenBytes\": {";
        bool first = true;
        for (const auto& [value, count] : stats.first_sixteen_bytes) {
            if (!first) output << ", "; output << '"' << value << "\": " << count; first = false;
        }
        output << "}, \"firstU32LE\": {"; first = true;
        for (const auto& [value, count] : stats.first_u32_le) {
            if (!first) output << ", "; output << '"' << value << "\": " << count; first = false;
        }
        output << "}, \"asciiLengthDistribution\": {"; first = true;
        for (const auto& [length, count] : stats.ascii_length_distribution) {
            if (!first) output << ", "; output << '"' << length << "\": " << count; first = false;
        }
        output << "}, \"prefixU32Fields\": {"; first = true;
        for (const auto& [offset, field] : stats.prefix_u32_fields) {
            if (!first) output << ", ";
            output << '"' << offset << "\": {\"observed\": " << field.observed
                   << ", \"distinctValues\": " << field.distinct_values.size()
                   << ", \"zeroValues\": " << field.zero_values
                   << ", \"valuesEqualRecordSize\": " << field.values_equal_record_size
                   << ", \"valuesEqualRemainingSize\": " << field.values_equal_remaining_size
                   << ", \"nonzeroValues\": " << field.nonzero_values
                   << ", \"valuesWithinRecordBounds\": " << field.values_within_record_bounds
                   << ", \"alignedValuesWithinRecordBounds\": " << field.aligned_values_within_record_bounds
                   << ", \"valuesWithinProfilePrefix\": " << field.values_within_profile_prefix
                   << ", \"valuesWithinPayloadRegion\": " << field.values_within_payload_region
                   << ", \"headerStability\": {\"observed\": " << field.headers_observed
                   << ", \"constantWithinHeader\": " << field.headers_constant
                   << ", \"distinctConstants\": " << field.distinct_header_constants.size() << "}"
                   << ", \"minimum\": " << field.minimum << ", \"maximum\": " << field.maximum << '}';
            first = false;
        }
        output << "}, \"candidateFields\": {\"constant\": ["; first = true;
        for (const auto& [offset, field] : stats.prefix_u32_fields) {
            if (field.observed != stats.records || field.distinct_values.size() != 1) continue;
            if (!first) output << ", "; output << offset; first = false;
        }
        output << "], \"crossHeaderConstant\": ["; first = true;
        for (const auto& [offset, field] : stats.prefix_u32_fields) {
            if (field.headers_observed != stats.headers_observed ||
                field.headers_constant != field.headers_observed ||
                field.distinct_header_constants.size() != 1) continue;
            if (!first) output << ", "; output << offset; first = false;
        }
        output << "], \"headerVariantConstant\": ["; first = true;
        for (const auto& [offset, field] : stats.prefix_u32_fields) {
            if (field.headers_observed != stats.headers_observed ||
                field.headers_constant != field.headers_observed ||
                field.distinct_header_constants.size() <= 1) continue;
            if (!first) output << ", "; output << offset; first = false;
        }
        output << "], \"recordVariable\": ["; first = true;
        for (const auto& [offset, field] : stats.prefix_u32_fields) {
            if (field.headers_observed != stats.headers_observed ||
                field.headers_constant == field.headers_observed) continue;
            if (!first) output << ", "; output << offset; first = false;
        }
        output << "], \"incompleteHeaderCoverage\": ["; first = true;
        for (const auto& [offset, field] : stats.prefix_u32_fields) {
            if (field.headers_observed == stats.headers_observed) continue;
            if (!first) output << ", "; output << offset; first = false;
        }
        output << "], \"boundedOffset\": ["; first = true;
        for (const auto& [offset, field] : stats.prefix_u32_fields) {
            if (field.observed == 0 || field.nonzero_values != field.observed ||
                field.values_within_record_bounds != field.observed) continue;
            if (!first) output << ", "; output << offset; first = false;
        }
        output << "], \"alignedBoundedOffset\": ["; first = true;
        for (const auto& [offset, field] : stats.prefix_u32_fields) {
            if (field.observed == 0 || field.nonzero_values != field.observed ||
                field.aligned_values_within_record_bounds != field.observed) continue;
            if (!first) output << ", "; output << offset; first = false;
        }
        output << "], \"recordSize\": {"; first = true;
        for (const auto& [offset, field] : stats.prefix_u32_fields) {
            if (!field.values_equal_record_size) continue;
            if (!first) output << ", "; output << '"' << offset << "\": " << field.values_equal_record_size; first = false;
        }
        output << "}, \"remainingSize\": {"; first = true;
        for (const auto& [offset, field] : stats.prefix_u32_fields) {
            if (!field.values_equal_remaining_size) continue;
            if (!first) output << ", "; output << '"' << offset << "\": " << field.values_equal_remaining_size; first = false;
        }
        output << "}}}"; first_family = false;
    }
    if (!catalog.families.empty()) output << '\n';
    output << "  },\n  \"errors\": [";
    for (std::size_t index = 0; index < catalog.errors.size(); ++index) {
        if (index) output << ','; output << "\n    \"" << escape_json(catalog.errors[index]) << '"';
    }
    if (!catalog.errors.empty()) output << "\n  ";
    output << "]\n}\n";
    return output.str();
}

} // namespace rb4pc::builder