#include "rb4pc/inspector/texture_container.hpp"
#include "rb4pc/inspector/sha256.hpp"

#include <array>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <utility>

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
        ? (result.platform_version == 0x0c ? 0x7cU : 0xacU)
        : (result.platform_version == 0x08 ? 0xa4U : 0xa8U);
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

TextureCorpus inspect_texture_corpus(const std::filesystem::path& root, std::string extension) {
    if (!std::filesystem::is_directory(root)) throw std::runtime_error("texture corpus root is not a directory");
    if (extension.empty() || extension.front() != '.') throw std::runtime_error("texture corpus extension must begin with a dot");
    TextureCorpus result;
    result.root = root;
    result.extension = std::move(extension);
    using FamilyKey = std::tuple<std::uint32_t, std::uint32_t, std::int32_t, std::int32_t,
                                 std::int32_t, std::uint32_t, std::uint32_t, std::uint32_t>;
    std::map<FamilyKey, std::uint64_t> families;
    std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint64_t> rejected;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file() || entry.path().extension().string() != result.extension) continue;
        ++result.candidate_count;
        std::ifstream signature(entry.path(), std::ios::binary);
        if (!signature) continue;
        std::uint32_t magic = 0;
        std::uint32_t version = 0;
        try {
            magic = read_u32(signature);
            version = read_u32(signature);
            const auto texture = inspect_texture_container(entry.path());
            const auto& first = texture.mipmaps.front();
            const auto pixels = static_cast<std::uint64_t>(first.width) * static_cast<std::uint64_t>(first.height);
            const auto bpp = pixels == 0 ? 0U : static_cast<std::uint32_t>(first.data_size * 8ULL / pixels);
            ++families[{texture.magic, texture.platform_version, first.width, first.height, first.flags,
                        static_cast<std::uint32_t>(texture.mipmaps.size()), texture.marker, bpp}];
            ++result.parsed_count;
        } catch (const std::exception&) {
            ++rejected[{magic, version}];
        }
    }
    for (const auto& [key, count] : families) {
        const auto& [magic, version, width, height, flags, levels, marker, bpp] = key;
        result.families.push_back({magic, version, width, height, flags, levels, marker, bpp, count});
    }
    for (const auto& [key, count] : rejected)
        result.rejected_signatures.push_back({key.first, key.second, count});
    return result;
}

std::string format_texture_corpus_report(const TextureCorpus& corpus) {
    std::ostringstream output;
    output << "Texture corpus: " << corpus.root.string() << '\n'
           << "Extension: " << corpus.extension << '\n'
           << "Candidates: " << corpus.candidate_count << '\n'
           << "Parsed: " << corpus.parsed_count << '\n'
           << "Families: " << corpus.families.size() << '\n';
    for (const auto& family : corpus.families) {
        output << "  count=" << family.file_count << ", magic=" << family.magic
               << ", version=0x" << std::hex << std::uppercase << family.platform_version << std::dec
               << ", base=" << family.width << 'x' << family.height
               << ", flags=" << family.flags << ", mipmaps=" << family.mipmap_count
               << ", marker=" << family.marker << ", bpp=" << family.bits_per_pixel << '\n';
    }
    output << "Rejected signatures: " << corpus.rejected_signatures.size() << '\n';
    for (const auto& rejected : corpus.rejected_signatures) {
        output << "  count=" << rejected.file_count << ", magic=" << rejected.magic
               << ", version=0x" << std::hex << std::uppercase << rejected.platform_version << std::dec << '\n';
    }
    return output.str();
}
} // namespace rb4pc::inspector
