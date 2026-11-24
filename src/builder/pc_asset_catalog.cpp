#include "rb4pc/builder/pc_asset_catalog.hpp"

#include "rb4pc/inspector/file_inspector.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace rb4pc::builder {
namespace {

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string extension_of(const std::string& path) {
    const auto normalized = lower(path);
    if (normalized.find(".fbx++mesh++") != std::string::npos) return ".fbx++mesh++*_pc";
    const auto slash = path.find_last_of('/');
    const auto dot = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return "<none>";
    return lower(path.substr(dot));
}

std::string namespace_of(const std::string& path) {
    const auto first = path.find('/');
    if (first == std::string::npos) return "<root>";
    const auto second = path.find('/', first + 1);
    return second == std::string::npos ? path : path.substr(0, second);
}

bool has_suffix(const std::string& path, const std::initializer_list<std::string_view>& suffixes) {
    for (const auto suffix : suffixes) if (path.ends_with(suffix)) return true;
    return false;
}

std::string category_of(const std::string& path) {
    const auto normalized = lower(path);
    if (normalized.starts_with("__marker__") || normalized == "__split_ark__") return "marker";
    if (has_suffix(normalized, {".mid_pc", ".midisong_pc", ".rbmid_pc"})) return "chart";
    if (has_suffix(normalized, {".mogg", ".wav", ".bank", ".fev", ".noises_pc"})) return "audio";
    if (has_suffix(normalized, {".script_dta_pc"})) return "script";
    if (normalized.find("_dta_pc") != std::string::npos ||
        has_suffix(normalized, {".rbsong", ".songdta_pc", ".csv_pc", ".dta"})) return "metadata";
    if (has_suffix(normalized, {".png_pc", ".bmp_pc", ".texture_pc", ".texarray2d_pc"})) return "texture";
    if (has_suffix(normalized, {".charanim_pc", ".charclip_pc", ".char_pc", ".skel_pc",
                          ".bonemask_pc", ".stance_pc"}))
        return "character-animation";
    if (has_suffix(normalized, {".lipsync_pc"})) return "lipsync";
    if (has_suffix(normalized, {".licks_pc"})) return "performance-pattern";
    if (has_suffix(normalized, {".layout_pc", ".font_pc", ".fontfam_pc"})) return "ui";
    if (has_suffix(normalized, {".h264"})) return "video";
    if (has_suffix(normalized, {".entity_pc", ".scene_pc", ".sgraph_pc", ".multifusion_pc",
                          ".fusion_dta_pc", ".mesh_pc", ".geom_pc", ".hlsl_pc"}))
        return "scene-rendering";
    if (normalized.find(".fbx++mesh++") != std::string::npos) return "scene-rendering";
    return "unknown";
}

enum class PathStatus { native, intermediate, unknown };

PathStatus status_of(const std::string& path, const std::string& category) {
    const auto normalized = lower(path);
    if (category == "marker") return PathStatus::native;
    if (has_suffix(normalized, {".mid", ".json", ".mogg.dta"})) return PathStatus::intermediate;
    if (category != "unknown") return PathStatus::native;
    return PathStatus::unknown;
}

std::string escape_json(std::string_view value) {
    std::string escaped;
    for (const char c : value) {
        if (c == '\\' || c == '"') escaped.push_back('\\');
        if (c == '\n') escaped += "\\n";
        else if (c == '\r') escaped += "\\r";
        else if (c == '\t') escaped += "\\t";
        else if (static_cast<unsigned char>(c) >= 0x20) escaped.push_back(c);
    }
    return escaped;
}

void write_map(std::ostringstream& output, const std::map<std::string, std::size_t>& values) {
    output << '{';
    bool first = true;
    for (const auto& [key, count] : values) {
        if (!first) output << ',';
        output << "\n      \"" << escape_json(key) << "\": " << count;
        first = false;
    }
    if (!values.empty()) output << '\n' << "    ";
    output << '}';
}

} // namespace

PcAssetCatalog catalog_pc_assets(const std::filesystem::path& source) {
    std::vector<std::filesystem::path> headers;
    if (std::filesystem::is_regular_file(source)) {
        if (lower(source.extension().string()) != ".hdr")
            throw std::runtime_error("PC asset catalog source file is not an HDR");
        headers.push_back(source);
    } else if (std::filesystem::is_directory(source)) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(source))
            if (entry.is_regular_file() && lower(entry.path().extension().string()) == ".hdr")
                headers.push_back(entry.path());
    } else throw std::runtime_error("PC asset catalog source does not exist");
    std::sort(headers.begin(), headers.end());
    if (headers.empty()) throw std::runtime_error("PC asset catalog found no HDR files");

    PcAssetCatalog catalog;
    catalog.candidate_headers = headers.size();
    for (const auto& header : headers) {
        try {
            const auto report = inspector::inspect_header_set(header);
            ++catalog.valid_headers;
            catalog.archive_volumes += report.metadata.archives.size();
            catalog.records += report.metadata.file_records.size();
            for (const auto& record : report.metadata.file_records) {
                catalog.payload_bytes += record.size;
                const auto extension = extension_of(record.path);
                const auto category = category_of(record.path);
                ++catalog.extensions[extension];
                ++catalog.namespaces[namespace_of(record.path)];
                ++catalog.categories[category];
                if (category == "unknown") ++catalog.unknown_extensions[extension];
                if (category == "marker") ++catalog.marker_records;
                switch (status_of(record.path, category)) {
                    case PathStatus::native: ++catalog.native_path_records; break;
                    case PathStatus::intermediate: ++catalog.intermediate_path_records; break;
                    case PathStatus::unknown: ++catalog.unknown_path_records; break;
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

std::string format_json(const PcAssetCatalog& catalog) {
    std::ostringstream output;
    output << "{\n  \"schemaVersion\": 1,\n"
           << "  \"headers\": {\"candidates\": " << catalog.candidate_headers
           << ", \"valid\": " << catalog.valid_headers
           << ", \"invalid\": " << catalog.invalid_headers << "},\n"
           << "  \"archiveVolumes\": " << catalog.archive_volumes << ",\n"
           << "  \"records\": {\"total\": " << catalog.records
           << ", \"payloadBytes\": " << catalog.payload_bytes
           << ", \"markers\": " << catalog.marker_records
           << ", \"nativePathFamily\": " << catalog.native_path_records
           << ", \"intermediatePathFamily\": " << catalog.intermediate_path_records
           << ", \"unknownPathFamily\": " << catalog.unknown_path_records << "},\n"
           << "  \"namespaces\": ";
    write_map(output, catalog.namespaces);
    output << ",\n  \"extensions\": ";
    write_map(output, catalog.extensions);
    output << ",\n  \"categories\": ";
    write_map(output, catalog.categories);
    output << ",\n  \"unknownExtensions\": ";
    write_map(output, catalog.unknown_extensions);
    output << ",\n  \"errors\": [";
    for (std::size_t index = 0; index < catalog.errors.size(); ++index) {
        if (index != 0) output << ',';
        output << "\n    \"" << escape_json(catalog.errors[index]) << '"';
    }
    if (!catalog.errors.empty()) output << '\n' << "  ";
    output << "]\n}\n";
    return output.str();
}

} // namespace rb4pc::builder
