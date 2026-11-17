#include "rb4pc/inspector/file_inspector.hpp"
#include "rb4pc/inspector/sha256.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace rb4pc::inspector {
namespace {

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

    const auto directory = path.has_parent_path() ? path.parent_path() : std::filesystem::path{"."};
    const auto escaped_stem = std::regex_replace(path.stem().string(), std::regex{R"([.^$|()\[\]{}*+?\\])"}, R"(\$&)");
    const std::regex pattern{"^" + escaped_stem + R"(_([0-9]+)\.ark$)", std::regex::icase};

    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file()) continue;
        std::smatch match;
        const auto name = entry.path().filename().string();
        if (!std::regex_match(name, match, pattern)) continue;
        report.archives.push_back({entry.path(), entry.file_size(), std::stoull(match[1].str())});
    }
    std::sort(report.archives.begin(), report.archives.end(),
              [](const ArchiveReport& left, const ArchiveReport& right) { return left.index < right.index; });
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
    output << format_report(report.header);
    output << "Platform hint: " << report.platform_hint << " (filename only)\n";
    output << std::fixed << std::setprecision(6);
    output << "Shannon entropy: " << report.analysis.entropy << " bits/byte\n";
    output << "Zero bytes: " << report.analysis.zero_bytes << '\n';
    output << "Analysis blocks: " << report.analysis.blocks.size() << " x up to "
           << report.analysis.block_size << " bytes\n";
    output << "Repeated full blocks: " << report.analysis.repeated_full_blocks << '\n';
    output << "Associated ARK volumes: " << report.archives.size() << '\n';
    for (const auto& archive : report.archives) {
        output << "  [" << archive.index << "] " << archive.path.filename().string()
               << " — " << archive.size << " bytes\n";
    }
    return output.str();
}

std::string format_json(const HeaderSetReport& report) {
    std::ostringstream output;
    output << "{\n";
    output << "  \"schemaVersion\": 1,\n";
    output << "  \"platformHint\": \"" << escape_json(report.platform_hint) << "\",\n";
    output << "  \"platformHintSource\": \"filename\",\n";
    output << "  \"header\": {\n";
    output << "    \"name\": \"" << escape_json(report.header.path.filename().string()) << "\",\n";
    output << "    \"size\": " << report.header.size << ",\n";
    output << "    \"sha256\": \"" << report.header.sha256 << "\",\n";
    output << "    \"firstBytesHex\": \"" << preview_hex(report.header) << "\"\n";
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
        output << "    {\"index\": " << archive.index
               << ", \"name\": \"" << escape_json(archive.path.filename().string())
               << "\", \"size\": " << archive.size << "}";
    }
    if (!report.archives.empty()) output << '\n';
    output << "  ]\n";
    output << "}\n";
    return output.str();
}

} // namespace rb4pc::inspector

