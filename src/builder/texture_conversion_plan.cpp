#include "rb4pc/builder/texture_conversion_plan.hpp"
#include "rb4pc/inspector/texture_container.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace rb4pc::builder {
namespace {
std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return value;
}

bool is_texture_candidate(const std::filesystem::path& path) {
    const auto name = lowercase(path.filename().string());
    return name.ends_with(".png_ps4") || name.ends_with(".texture");
}

std::string escape_json(std::string_view value) {
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
                    constexpr char digits[] = "0123456789abcdef";
                    output << "\\u00" << digits[character >> 4] << digits[character & 0x0f];
                } else output << static_cast<char>(character);
        }
    }
    return output.str();
}

const char* state_name(TextureConversionState state) {
    switch (state) {
        case TextureConversionState::convertible: return "convertible";
        case TextureConversionState::read_only: return "readOnly";
        case TextureConversionState::invalid: return "invalid";
    }
    return "invalid";
}
} // namespace

TextureConversionPlan plan_texture_conversions(const std::filesystem::path& root) {
    if (!std::filesystem::is_directory(root))
        throw std::runtime_error("texture conversion plan root is not a directory");
    TextureConversionPlan plan;
    plan.root = root;
    for (const auto& item : std::filesystem::recursive_directory_iterator(root)) {
        if (!item.is_regular_file() || !is_texture_candidate(item.path())) continue;
        TextureConversionPlanEntry entry;
        entry.relative_path = std::filesystem::relative(item.path(), root);
        try {
            const auto texture = inspector::inspect_texture_container(item.path());
            entry.magic = texture.magic;
            entry.platform_version = texture.platform_version;
            entry.mipmap_count = static_cast<std::uint32_t>(texture.mipmaps.size());
            if (!texture.mipmaps.empty()) {
                entry.width = static_cast<std::uint32_t>(texture.mipmaps.front().width);
                entry.height = static_cast<std::uint32_t>(texture.mipmaps.front().height);
            }
            const auto compatibility = inspector::assess_linear_ps4_texture_conversion(texture);
            entry.reason = compatibility.reason;
            if (compatibility.compatible) {
                entry.state = TextureConversionState::convertible;
                const auto pixels = static_cast<std::uint64_t>(entry.width) * entry.height;
                entry.bits_per_pixel = pixels
                    ? static_cast<std::uint32_t>(texture.mipmaps.front().data_size * 8ULL / pixels) : 0;
                ++plan.convertible;
            } else {
                entry.state = TextureConversionState::read_only;
                ++plan.read_only;
            }
        } catch (const std::exception& error) {
            entry.state = TextureConversionState::invalid;
            entry.reason = error.what();
            ++plan.invalid;
        }
        plan.entries.push_back(std::move(entry));
    }
    std::sort(plan.entries.begin(), plan.entries.end(), [](const auto& left, const auto& right) {
        return left.relative_path.generic_string() < right.relative_path.generic_string();
    });
    return plan;
}

std::string format_json(const TextureConversionPlan& plan) {
    std::ostringstream output;
    output << "{\n  \"schemaVersion\": 1,\n  \"root\": \""
           << escape_json(plan.root.generic_string()) << "\",\n"
           << "  \"candidateCount\": " << plan.entries.size() << ",\n"
           << "  \"convertible\": " << plan.convertible << ",\n"
           << "  \"readOnly\": " << plan.read_only << ",\n"
           << "  \"invalid\": " << plan.invalid << ",\n  \"entries\": [";
    for (std::size_t index = 0; index < plan.entries.size(); ++index) {
        const auto& entry = plan.entries[index];
        if (index) output << ',';
        output << "\n    {\"path\": \"" << escape_json(entry.relative_path.generic_string())
               << "\", \"state\": \"" << state_name(entry.state)
               << "\", \"reason\": \"" << escape_json(entry.reason)
               << "\", \"magic\": " << entry.magic
               << ", \"platformVersion\": " << entry.platform_version
               << ", \"width\": " << entry.width << ", \"height\": " << entry.height
               << ", \"mipmaps\": " << entry.mipmap_count
               << ", \"bitsPerPixel\": " << entry.bits_per_pixel << '}';
    }
    output << "\n  ]\n}\n";
    return output.str();
}
std::filesystem::path pc_texture_relative_path(const std::filesystem::path& source_relative_path) {
    auto destination = source_relative_path;
    auto name = destination.filename().string();
    const auto lower_name = lowercase(name);
    if (lower_name.ends_with("_ps4")) name.replace(name.size() - 4, 4, "_pc");
    else name += "_pc";
    destination.replace_filename(name);
    return destination;
}

std::string format_json(const TextureBatchResult& result) {
    std::ostringstream output;
    output << "{\n  \"schemaVersion\": 1,\n  \"converted\": " << result.items.size()
           << ",\n  \"skippedReadOnly\": " << result.skipped_read_only
           << ",\n  \"skippedInvalid\": " << result.skipped_invalid << ",\n  \"items\": [";
    for (std::size_t index = 0; index < result.items.size(); ++index) {
        const auto& item = result.items[index];
        if (index) output << ',';
        output << "\n    {\"source\": \"" << escape_json(item.source_relative_path.generic_string())
               << "\", \"destination\": \"" << escape_json(item.destination_relative_path.generic_string())
               << "\", \"bitsPerPixel\": " << item.bits_per_pixel
               << ", \"sourceSha256\": \"" << item.source_sha256
               << "\", \"destinationSha256\": \"" << item.destination_sha256
               << "\", \"payloadSha256\": \"" << item.payload_sha256 << "\"}";
    }
    output << "\n  ]\n}\n";
    return output.str();
}

TextureBatchResult convert_texture_batch(const std::filesystem::path& source_root,
                                         const std::filesystem::path& destination_root) {
    if (destination_root.empty() || std::filesystem::exists(destination_root))
        throw std::runtime_error("texture batch destination is empty or already exists");
    const auto parent = destination_root.has_parent_path()
        ? destination_root.parent_path() : std::filesystem::path{"."};
    if (!std::filesystem::is_directory(parent))
        throw std::runtime_error("texture batch destination parent does not exist");
    auto temporary = destination_root;
    temporary += ".rb4pc-part";
    if (std::filesystem::exists(temporary))
        throw std::runtime_error("texture batch temporary destination already exists");

    const auto plan = plan_texture_conversions(source_root);
    TextureBatchResult result;
    result.directory = destination_root;
    result.summary = destination_root / "batch-summary.json";
    result.skipped_read_only = plan.read_only;
    result.skipped_invalid = plan.invalid;
    try {
        std::filesystem::create_directory(temporary);
        for (const auto& entry : plan.entries) {
            if (entry.state != TextureConversionState::convertible) continue;
            const auto destination_relative = pc_texture_relative_path(entry.relative_path);
            const auto temporary_destination = temporary / destination_relative;
            std::filesystem::create_directories(temporary_destination.parent_path());
            const auto conversion = inspector::convert_linear_ps4_texture_to_pc(
                source_root / entry.relative_path, temporary_destination);
            result.items.push_back({entry.relative_path, destination_relative,
                                    conversion.bits_per_pixel, conversion.source_sha256,
                                    conversion.destination_sha256, conversion.payload_sha256});
        }
        const auto temporary_summary = temporary / "batch-summary.json";
        std::ofstream output(temporary_summary, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("could not create texture batch summary");
        output << format_json(result);
        output.close();
        if (!output) throw std::runtime_error("could not finalize texture batch summary");
        std::filesystem::rename(temporary, destination_root);
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove_all(temporary, ignored);
        throw;
    }
    return result;
}
} // namespace rb4pc::builder