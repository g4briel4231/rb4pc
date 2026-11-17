#include "rb4pc/builder/custom_song_catalog.hpp"
#include "rb4pc/builder/song_layout.hpp"
#include "rb4pc/inspector/hdr_decoder.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace rb4pc::builder {
namespace {
bool ends_with(const std::string& value, const std::string& suffix) { return value.size() >= suffix.size() && value.ends_with(suffix); }
std::string escape_json(const std::string& value) {
    std::string out; for (const char c : value) { if (c == '"' || c == '\\') out += '\\'; out += c; } return out;
}
}
CustomSongCatalog scan_custom_songs(const std::filesystem::path& installation) {
    if (!std::filesystem::is_directory(installation)) throw std::runtime_error("custom-song installation is not a directory");
    CustomSongCatalog catalog; catalog.installation = installation;
    for (const auto& item : std::filesystem::directory_iterator(installation)) {
        if (!item.is_regular_file()) continue;
        const auto name = item.path().filename().string();
        if (!ends_with(name, "_pc.hdr") || name == "main_pc.hdr") continue;
        CustomSongEntry entry; entry.short_name = name.substr(0, name.size() - 7); entry.header = item.path();
        try {
            const auto paths = custom_song_package_paths(entry.short_name);
            entry.archive = installation / paths.archive;
            const auto metadata = inspector::decode_hdr_prefix(entry.header);
            entry.record_count = metadata.file_records.size();
            entry.header_valid = metadata.version == 10 && metadata.hash_index_validated &&
                                 metadata.archives.size() == 1 && metadata.archives[0].name == paths.archive.generic_string();
            entry.archive_present = std::filesystem::is_regular_file(entry.archive);
            entry.archive_usable = entry.archive_present && !metadata.archives.empty() &&
                std::filesystem::file_size(entry.archive) >= metadata.archives[0].declared_size;
            entry.ready = entry.header_valid && entry.archive_usable;
        } catch (const std::exception& error) { entry.error = error.what(); }
        if (entry.ready) ++catalog.ready_songs;
        catalog.songs.push_back(std::move(entry));
    }
    std::sort(catalog.songs.begin(), catalog.songs.end(), [](const auto& a, const auto& b) { return a.short_name < b.short_name; });
    return catalog;
}

std::string format_json(const CustomSongCatalog& catalog) {
    std::ostringstream out; out << "{\n  \"schemaVersion\": 1,\n  \"installation\": \""
        << escape_json(catalog.installation.string()) << "\",\n  \"songCount\": " << catalog.songs.size()
        << ",\n  \"readySongCount\": " << catalog.ready_songs << ",\n  \"songs\": [";
    for (std::size_t i = 0; i < catalog.songs.size(); ++i) {
        const auto& s = catalog.songs[i]; out << (i ? ",\n" : "\n") << "    {\"shortName\": \""
            << escape_json(s.short_name) << "\", \"header\": \"" << escape_json(s.header.filename().string())
            << "\", \"archive\": \"" << escape_json(s.archive.filename().string()) << "\", \"records\": "
            << s.record_count << ", \"ready\": " << (s.ready ? "true" : "false");
        if (!s.error.empty()) out << ", \"error\": \"" << escape_json(s.error) << "\"";
        out << '}';
    }
    if (!catalog.songs.empty()) out << '\n';
    out << "  ]\n}\n";
    return out.str();
}
} // namespace rb4pc::builder
