#include "rb4pc/inspector/texture_container.hpp"
#include "rb4pc/inspector/sha256.hpp"

#include <array>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace rb4pc::inspector {
namespace {
std::uint32_t read_u32(std::ifstream& input) {
    std::array<unsigned char, 4> bytes{};
    input.read(reinterpret_cast<char*>(bytes.data()), 4);
    if (!input) throw std::runtime_error("truncated texture container");
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8) |
           (static_cast<std::uint32_t>(bytes[2]) << 16) |
           (static_cast<std::uint32_t>(bytes[3]) << 24);
}

void skip(std::ifstream& input, std::uint64_t count) {
    if (count > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max()))
        throw std::runtime_error("texture offset is too large");
    input.seekg(static_cast<std::streamoff>(count), std::ios::cur);
    if (!input) throw std::runtime_error("truncated texture container");
}
} // namespace

TextureContainer inspect_texture_container(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("could not open texture container: " + path.string());
    input.seekg(0, std::ios::end);
    const auto end = input.tellg();
    if (end < 0) throw std::runtime_error("could not determine texture size");
    const auto file_size = static_cast<std::uint64_t>(end);
    input.seekg(0);

    TextureContainer result;
    result.path = path;
    result.magic = read_u32(input);
    result.platform_version = read_u32(input);
    if (result.magic != 6 && result.magic != 4)
        throw std::runtime_error("unsupported texture magic " + std::to_string(result.magic));
    result.header_data_size = result.magic == 6
        ? (result.platform_version == 0x0c ? 0x7cU : 0xacU) : 0xa8U;
    result.mipmap_entry_size = result.platform_version == 0x0c ? 20U : 12U;
    skip(input, result.header_data_size);
    const auto level_count = read_u32(input);
    if (level_count == 0 || level_count > 32) throw std::runtime_error("invalid texture mipmap count");
    result.mipmaps.reserve(level_count);
    for (std::uint32_t index = 0; index < level_count; ++index) {
        TextureMipmap mip;
        mip.width = static_cast<std::int32_t>(read_u32(input));
        mip.height = static_cast<std::int32_t>(read_u32(input));
        mip.flags = static_cast<std::int32_t>(read_u32(input));
        if (result.platform_version == 0x0c) skip(input, 8);
        if (mip.width <= 0 || mip.height <= 0) throw std::runtime_error("invalid texture dimensions");
        result.mipmaps.push_back(mip);
    }
    result.marker = read_u32(input);
    for (auto& mip : result.mipmaps) {
        mip.data_size = read_u32(input);
        const auto position = input.tellg();
        if (position < 0) throw std::runtime_error("could not locate texture payload");
        mip.data_offset = static_cast<std::uint64_t>(position);
        if (mip.data_offset + mip.data_size > file_size) throw std::runtime_error("texture payload exceeds file size");
        mip.sha256 = sha256_range(input, mip.data_offset, mip.data_size);
        input.clear();
        input.seekg(static_cast<std::streamoff>(mip.data_offset + mip.data_size));
    }
    const auto payload_end = input.tellg();
    if (payload_end < 0 || static_cast<std::uint64_t>(payload_end) > file_size)
        throw std::runtime_error("invalid texture payload boundary");
    const auto remaining = file_size - static_cast<std::uint64_t>(payload_end);
    if (remaining != 0x1c) throw std::runtime_error("unexpected texture footer size " + std::to_string(remaining));
    result.footer_size = static_cast<std::uint32_t>(remaining);
    return result;
}

TextureCompatibility compare_texture_payloads(const TextureContainer& source,
                                               const TextureContainer& reference) {
    if (source.mipmaps.size() != reference.mipmaps.size()) return {false, "mipmap count differs"};
    for (std::size_t i = 0; i < source.mipmaps.size(); ++i) {
        const auto& left = source.mipmaps[i];
        const auto& right = reference.mipmaps[i];
        if (left.width != right.width || left.height != right.height) return {false, "mipmap dimensions differ"};
        if (left.flags != right.flags) return {false, "mipmap flags differ"};
        if (left.data_size != right.data_size) return {false, "mipmap payload size differs"};
        if (left.sha256 != right.sha256) return {false, "mipmap payload bytes differ"};
    }
    return {true, "all mipmap dimensions, flags, sizes, and payload hashes match"};
}

std::string format_texture_report(const TextureContainer& texture) {
    std::ostringstream output;
    output << "Texture: " << texture.path.string() << '\n'
           << "Magic: " << texture.magic << '\n'
           << "Platform version: 0x" << std::hex << std::uppercase << texture.platform_version << std::dec << '\n'
           << "Marker: " << texture.marker << '\n'
           << "Header data: " << texture.header_data_size << " bytes\n"
           << "Mipmap entry: " << texture.mipmap_entry_size << " bytes\n"
           << "Mipmaps: " << texture.mipmaps.size() << '\n';
    for (std::size_t i = 0; i < texture.mipmaps.size(); ++i) {
        const auto& mip = texture.mipmaps[i];
        output << "  [" << i << "] " << mip.width << 'x' << mip.height
               << ", flags=" << mip.flags << ", size=" << mip.data_size
               << ", sha256=" << mip.sha256 << '\n';
    }
    output << "Footer: " << texture.footer_size << " bytes\n";
    return output.str();
}
} // namespace rb4pc::inspector
