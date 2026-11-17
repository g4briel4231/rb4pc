#include "rb4pc/inspector/music_catalog.hpp"
#include "rb4pc/inspector/file_inspector.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <vector>

namespace rb4pc::inspector {
namespace {
std::string target_type(const std::string& path) {
    for (const auto* extension : {".mid_pc", ".midisong_pc", ".mogg", ".rbmid_pc"})
        if (path.ends_with(extension)) return extension;
    return {};
}
std::string hex4(const unsigned char* data, std::size_t size) {
    std::ostringstream out; out << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < std::min<std::size_t>(4, size); ++i) out << std::setw(2) << static_cast<unsigned>(data[i]);
    return out.str();
}
}

MusicCatalog catalog_music_payloads(const HeaderSetReport& report) {
    MusicCatalog catalog;
    std::vector<std::ifstream> inputs; inputs.reserve(report.archives.size());
    for (const auto& archive : report.archives) inputs.emplace_back(archive.path, std::ios::binary);
    for (std::size_t index = 0; index < report.metadata.file_records.size(); ++index) {
        const auto& record = report.metadata.file_records[index];
        const auto type = target_type(record.path); if (type.empty()) continue;
        auto& stats = catalog.types[type];
        ++stats.records; stats.total_bytes += record.size;
        if (stats.records == 1) stats.minimum_size = record.size;
        else stats.minimum_size = std::min(stats.minimum_size, record.size);
        stats.maximum_size = std::max(stats.maximum_size, record.size);
        if (index >= report.record_locations.size() || !report.record_locations[index]) { ++catalog.unreadable; continue; }
        const auto location = *report.record_locations[index];
        auto& input = inputs[location.archive_index]; input.clear(); input.seekg(static_cast<std::streamoff>(location.local_offset));
        std::array<unsigned char, 32> bytes{}; const auto size = std::min<std::size_t>(bytes.size(), record.size);
        input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
        if (static_cast<std::size_t>(input.gcount()) != size) { ++catalog.unreadable; continue; }
        ++stats.first_four_bytes[hex4(bytes.data(), size)];
        if (size >= 4 && std::equal(bytes.begin(), bytes.begin() + 4, reinterpret_cast<const unsigned char*>("MThd"))) ++stats.standard_midi_headers;
        if (size >= 4 && std::equal(bytes.begin(), bytes.begin() + 4, reinterpret_cast<const unsigned char*>("OggS"))) ++stats.ogg_page_headers;
    }
    return catalog;
}

std::string format_report(const MusicCatalog& catalog) {
    std::ostringstream out; out << "Music payload catalog:\n";
    for (const auto& [type, s] : catalog.types) {
        out << "  " << type << ": records=" << s.records << ", bytes=" << s.total_bytes
            << ", min=" << s.minimum_size << ", max=" << s.maximum_size
            << ", MThd=" << s.standard_midi_headers << ", OggS=" << s.ogg_page_headers
            << ", distinctFirst4=" << s.first_four_bytes.size() << '\n';
    }
    out << "  Unreadable: " << catalog.unreadable << '\n'; return out.str();
}
} // namespace rb4pc::inspector
