#include "rb4pc/builder/texture_research.hpp"
#include "rb4pc/inspector/texture_container.hpp"

#include <sstream>
#include <string_view>

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
} // namespace

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
} // namespace rb4pc::builder