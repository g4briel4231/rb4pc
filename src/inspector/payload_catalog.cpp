#include "rb4pc/inspector/payload_catalog.hpp"

#include "rb4pc/inspector/file_inspector.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

namespace rb4pc::inspector {
namespace {

constexpr std::size_t signature_sample_size = 16;

bool starts_with(const unsigned char* data, std::size_t size,
                 const unsigned char* signature, std::size_t signature_size) {
    return size >= signature_size && std::equal(signature, signature + signature_size, data);
}

std::string normalized_extension(const std::string& path) {
    const auto separator = path.find_last_of("/\\");
    const auto dot = path.find_last_of('.');
    if (dot == std::string::npos || (separator != std::string::npos && dot < separator) || dot + 1 == path.size()) {
        return "<none>";
    }
    auto extension = path.substr(dot);
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return extension;
}

std::vector<std::pair<std::string, std::size_t>> sorted_counts(
    const std::map<std::string, std::size_t>& counts) {
    std::vector<std::pair<std::string, std::size_t>> result(counts.begin(), counts.end());
    std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
        if (left.second != right.second) return left.second > right.second;
        return left.first < right.first;
    });
    return result;
}

} // namespace

std::string classify_payload_signature(const unsigned char* data, std::size_t size) {
    static constexpr unsigned char png[] = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    static constexpr unsigned char jpeg[] = {0xff, 0xd8, 0xff};
    static constexpr unsigned char zip[] = {'P', 'K', 0x03, 0x04};
    static constexpr unsigned char gzip[] = {0x1f, 0x8b};
    static constexpr unsigned char elf[] = {0x7f, 'E', 'L', 'F'};
    if (starts_with(data, size, png, sizeof(png))) return "png";
    if (starts_with(data, size, jpeg, sizeof(jpeg))) return "jpeg";
    if (starts_with(data, size, zip, sizeof(zip))) return "zip";
    if (starts_with(data, size, gzip, sizeof(gzip))) return "gzip-candidate";
    if (starts_with(data, size, elf, sizeof(elf))) return "elf";
    if (size >= 4 && std::equal(data, data + 4, reinterpret_cast<const unsigned char*>("DDS "))) return "dds";
    if (size >= 4 && std::equal(data, data + 4, reinterpret_cast<const unsigned char*>("OggS"))) return "ogg";
    if (size >= 4 && std::equal(data, data + 4, reinterpret_cast<const unsigned char*>("RIFF"))) return "riff";
    if (size >= 4 && std::equal(data, data + 4, reinterpret_cast<const unsigned char*>("MThd"))) return "midi";
    if (size >= 2 && data[0] == 'M' && data[1] == 'Z') return "pe";
    if (size >= 2 && data[0] == 0x78 && ((static_cast<unsigned int>(data[0]) << 8) + data[1]) % 31 == 0) {
        return "zlib-candidate";
    }
    return "unknown";
}

PayloadCatalog catalog_payloads(const HeaderSetReport& report) {
    PayloadCatalog catalog;
    catalog.total_records = report.metadata.file_records.size();
    std::vector<std::ifstream> inputs;
    inputs.reserve(report.archives.size());
    for (const auto& archive : report.archives) inputs.emplace_back(archive.path, std::ios::binary);

    for (std::size_t index = 0; index < report.metadata.file_records.size(); ++index) {
        const auto& record = report.metadata.file_records[index];
        ++catalog.extensions[normalized_extension(record.path)];
        if (record.size == 0) {
            ++catalog.empty_records;
            continue;
        }
        if (index >= report.record_locations.size() || !report.record_locations[index]) {
            ++catalog.unreadable_records;
            continue;
        }
        const auto location = *report.record_locations[index];
        if (location.archive_index >= inputs.size() || !inputs[location.archive_index]) {
            ++catalog.unreadable_records;
            continue;
        }
        auto& input = inputs[location.archive_index];
        input.clear();
        input.seekg(static_cast<std::streamoff>(location.local_offset));
        std::array<unsigned char, signature_sample_size> sample{};
        const auto sample_size = std::min<std::size_t>(sample.size(), record.size);
        input.read(reinterpret_cast<char*>(sample.data()), static_cast<std::streamsize>(sample_size));
        if (static_cast<std::size_t>(input.gcount()) != sample_size) {
            ++catalog.unreadable_records;
            continue;
        }
        ++catalog.analyzed_records;
        const auto signature = classify_payload_signature(sample.data(), sample_size);
        ++catalog.signatures[signature];
        auto& examples = catalog.signature_examples[signature];
        if (examples.size() < 3) examples.push_back(record.path);
    }
    return catalog;
}

std::string format_report(const PayloadCatalog& catalog) {
    std::ostringstream output;
    output << "Payload catalog:\n";
    output << "  Total records: " << catalog.total_records << '\n';
    output << "  Analyzed non-empty records: " << catalog.analyzed_records << '\n';
    output << "  Empty records: " << catalog.empty_records << '\n';
    output << "  Unreadable records: " << catalog.unreadable_records << '\n';
    output << "  Signature groups:\n";
    for (const auto& [name, count] : sorted_counts(catalog.signatures)) {
        output << "    " << name << ": " << count;
        const auto examples = catalog.signature_examples.find(name);
        if (examples != catalog.signature_examples.end() && !examples->second.empty()) {
            output << " (examples: ";
            for (std::size_t index = 0; index < examples->second.size(); ++index) {
                if (index != 0) output << ", ";
                output << examples->second[index];
            }
            output << ')';
        }
        output << '\n';
    }
    output << "  Extension groups (top 25):\n";
    const auto extensions = sorted_counts(catalog.extensions);
    const auto limit = std::min<std::size_t>(extensions.size(), 25);
    for (std::size_t index = 0; index < limit; ++index) {
        output << "    " << extensions[index].first << ": " << extensions[index].second << '\n';
    }
    return output.str();
}

} // namespace rb4pc::inspector
