#include "rb4pc/builder/texture_conversion_plan.hpp"
#include "rb4pc/inspector/texture_container.hpp"

#include <algorithm>
#include <cctype>
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
} // namespace rb4pc::builder