#include "rb4pc/builder/texture_research.hpp"
#include "rb4pc/inspector/texture_container.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <tuple>

namespace rb4pc::builder {
namespace {
std::string escape_json(std::string_view value) {
    std::ostringstream output;
    for (const unsigned char character : value) {
        if (character == '"') output << "\\\"";
        else if (character == '\\') output << "\\\\";
        else if (character == '\n') output << "\\n";
        else if (character == '\r') output << "\\r";
        else if (character == '\t') output << "\\t";
        else output << static_cast<char>(character);
    }
    return output.str();
}

std::string pair_key(const std::filesystem::path& relative) {
    auto key = relative.generic_string();
    std::transform(key.begin(), key.end(), key.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    if (key.ends_with("_ps4")) key.resize(key.size() - 4);
    else if (key.ends_with("_pc")) key.resize(key.size() - 3);
    return key;
}

std::map<std::string, std::filesystem::path> texture_files(const std::filesystem::path& root) {
    if (!std::filesystem::is_directory(root)) throw std::runtime_error("texture pair root is not a directory");
    std::map<std::string, std::filesystem::path> files;
    for (const auto& item : std::filesystem::recursive_directory_iterator(root)) {
        if (!item.is_regular_file()) continue;
        const auto relative = std::filesystem::relative(item.path(), root);
        auto name = relative.filename().string();
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
        if (!name.ends_with(".png_ps4") && !name.ends_with(".png_pc") && !name.ends_with(".texture")) continue;
        files.emplace(pair_key(relative), relative);
    }
    return files;
}} // namespace

TexturePairReport compare_texture_pair(const std::filesystem::path& source,
                                       const std::filesystem::path& reference) {
    const auto source_texture = inspector::inspect_texture_container(source);
    const auto reference_texture = inspector::inspect_texture_container(reference);
    const auto compatibility = inspector::compare_texture_payloads(source_texture, reference_texture);
    TexturePairReport report;
    report.source = source;
    report.reference = reference;
    report.source_magic = source_texture.magic;
    report.source_version = source_texture.platform_version;
    report.reference_magic = reference_texture.magic;
    report.reference_version = reference_texture.platform_version;
    report.source_mipmaps = static_cast<std::uint32_t>(source_texture.mipmaps.size());
    report.reference_mipmaps = static_cast<std::uint32_t>(reference_texture.mipmaps.size());
    report.source_payload_sha256 = source_texture.payload_sha256;
    report.reference_payload_sha256 = reference_texture.payload_sha256;
    report.payload_compatible = compatibility.compatible;
    report.reason = compatibility.reason;
    return report;
}

std::string format_json(const TexturePairReport& report) {
    std::ostringstream output;
    output << "{\n  \"schemaVersion\": 1,\n"
           << "  \"source\": \"" << escape_json(report.source.generic_string()) << "\",\n"
           << "  \"reference\": \"" << escape_json(report.reference.generic_string()) << "\",\n"
           << "  \"sourceMagic\": " << report.source_magic << ",\n"
           << "  \"sourceVersion\": " << report.source_version << ",\n"
           << "  \"referenceMagic\": " << report.reference_magic << ",\n"
           << "  \"referenceVersion\": " << report.reference_version << ",\n"
           << "  \"sourceMipmaps\": " << report.source_mipmaps << ",\n"
           << "  \"referenceMipmaps\": " << report.reference_mipmaps << ",\n"
           << "  \"sourcePayloadSha256\": \"" << report.source_payload_sha256 << "\",\n"
           << "  \"referencePayloadSha256\": \"" << report.reference_payload_sha256 << "\",\n"
           << "  \"payloadCompatible\": " << (report.payload_compatible ? "true" : "false") << ",\n"
           << "  \"reason\": \"" << escape_json(report.reason) << "\"\n}\n";
    return output.str();
}

TextureDirectoryPairReport compare_texture_directories(const std::filesystem::path& source_root,
                                                       const std::filesystem::path& reference_root) {
    const auto sources = texture_files(source_root);
    const auto references = texture_files(reference_root);
    TextureDirectoryPairReport report;
    report.source_root = source_root;
    report.reference_root = reference_root;
    for (const auto& [key, source_relative] : sources) {
        const auto found = references.find(key);
        if (found == references.end()) { report.unmatched_sources.push_back(source_relative); continue; }
        auto pair = compare_texture_pair(source_root / source_relative, reference_root / found->second);
        if (pair.payload_compatible) ++report.compatible; else ++report.incompatible;
        report.pairs.push_back(std::move(pair));
    }
    for (const auto& [key, reference_relative] : references)
        if (!sources.contains(key)) report.unmatched_references.push_back(reference_relative);
    return report;
}

std::string format_json(const TextureDirectoryPairReport& report) {
    std::ostringstream output;
    output << "{\n  \"schemaVersion\": 1,\n  \"pairCount\": " << report.pairs.size()
           << ",\n  \"compatible\": " << report.compatible << ",\n  \"incompatible\": " << report.incompatible
           << ",\n  \"unmatchedSources\": " << report.unmatched_sources.size()
           << ",\n  \"unmatchedReferences\": " << report.unmatched_references.size() << ",\n  \"pairs\": [";
    for (std::size_t index = 0; index < report.pairs.size(); ++index) {
        const auto& pair = report.pairs[index]; if (index) output << ',';
        output << "\n    {\"source\": \"" << escape_json(pair.source.generic_string())
               << "\", \"reference\": \"" << escape_json(pair.reference.generic_string())
               << "\", \"payloadCompatible\": " << (pair.payload_compatible ? "true" : "false")
               << ", \"reason\": \"" << escape_json(pair.reason) << "\"}";
    }
    output << "\n  ],\n  \"unmatchedSourcePaths\": [";
    for (std::size_t i = 0; i < report.unmatched_sources.size(); ++i) { if (i) output << ','; output << "\n    \"" << escape_json(report.unmatched_sources[i].generic_string()) << '"'; }
    output << "\n  ],\n  \"unmatchedReferencePaths\": [";
    for (std::size_t i = 0; i < report.unmatched_references.size(); ++i) { if (i) output << ','; output << "\n    \"" << escape_json(report.unmatched_references[i].generic_string()) << '"'; }
    output << "\n  ]\n}\n"; return output.str();
}
TextureEvidenceReport profile_magic4_version8(const std::filesystem::path& root) {
    if (!std::filesystem::is_directory(root)) throw std::runtime_error("texture evidence root is not a directory");
    TextureEvidenceReport report; report.root = root;
    std::map<std::tuple<std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t,
                        std::uint32_t, std::uint32_t, std::uint32_t>, std::uint64_t> families;
    for (const auto& item : std::filesystem::recursive_directory_iterator(root)) {
        if (!item.is_regular_file()) continue;
        auto name = item.path().filename().string();
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (!name.ends_with(".texture") && !name.ends_with(".png_ps4") && !name.ends_with(".png_pc")) continue;
        ++report.candidates;
        try {
            const auto texture = inspector::inspect_texture_container(item.path());
            if (texture.magic != 4 || texture.platform_version != 8) { ++report.other_families; continue; }
            ++report.magic4_version8;
            const auto& first = texture.mipmaps.front();
            const auto pixels = static_cast<std::uint64_t>(first.width) * first.height;
            const auto bpp = pixels ? static_cast<std::uint32_t>(first.data_size * 8ULL / pixels) : 0;
            ++families[{static_cast<std::uint32_t>(first.width), static_cast<std::uint32_t>(first.height),
                        static_cast<std::uint32_t>(texture.mipmaps.size()), texture.header_data_size,
                        texture.mipmap_entry_size, texture.marker, bpp}];
        } catch (...) { ++report.unreadable; }
    }
    for (const auto& [key, count] : families) {
        const auto [width, height, mipmaps, header, entry, marker, bpp] = key;
        report.families.push_back({width, height, mipmaps, header, entry, marker, bpp, count});
    }
    return report;
}

std::string format_json(const TextureEvidenceReport& report) {
    std::ostringstream output;
    output << "{\n  \"schemaVersion\": 1,\n  \"candidates\": " << report.candidates
           << ",\n  \"magic4Version8\": " << report.magic4_version8
           << ",\n  \"otherFamilies\": " << report.other_families
           << ",\n  \"unreadable\": " << report.unreadable << ",\n  \"families\": [";
    for (std::size_t i = 0; i < report.families.size(); ++i) {
        const auto& f = report.families[i]; if (i) output << ',';
        output << "\n    {\"width\": " << f.width << ", \"height\": " << f.height
               << ", \"mipmaps\": " << f.mipmaps << ", \"headerBytes\": " << f.header_bytes
               << ", \"mipmapEntryBytes\": " << f.mipmap_entry_bytes << ", \"marker\": " << f.marker
               << ", \"bitsPerPixel\": " << f.bits_per_pixel << ", \"files\": " << f.files << '}';
    }
    output << "\n  ]\n}\n"; return output.str();
}} // namespace rb4pc::builder