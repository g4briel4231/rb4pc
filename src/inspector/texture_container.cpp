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

void set_u32(std::array<unsigned char, 172>& bytes, std::size_t offset, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8)
        bytes[offset + static_cast<std::size_t>(shift / 8)] = static_cast<unsigned char>(value >> shift);
}

void write_u32(std::ofstream& output, std::uint32_t value) {
    std::array<unsigned char, 4> bytes{};
    for (int shift = 0; shift < 32; shift += 8)
        bytes[static_cast<std::size_t>(shift / 8)] = static_cast<unsigned char>(value >> shift);
    output.write(reinterpret_cast<const char*>(bytes.data()), 4);
}

std::array<unsigned char, 172> canonical_pc_header(std::uint32_t bits_per_pixel) {
    std::array<unsigned char, 172> header{};
    set_u32(header, 0x08, 0x10);
    set_u32(header, 0x1c, 0x04);
    set_u32(header, 0x24, 0x20);
    set_u32(header, 0x58, 0x06);
    set_u32(header, 0x5c, 0x10);
    set_u32(header, 0x64, bits_per_pixel == 8 ? 3U : 1U);
    set_u32(header, 0x68, bits_per_pixel == 8 ? 3U : 1U);
    set_u32(header, 0x70, 0xffffffffU);
    set_u32(header, 0x74, 0xffffffffU);
    set_u32(header, 0x78, bits_per_pixel == 8 ? 8U : 4U);
    set_u32(header, 0x7c, 0x02);
    set_u32(header, 0x80, bits_per_pixel == 8 ? 4U : 1U);
    set_u32(header, 0x84, 0x02);
    set_u32(header, 0x88, 0x02);
    set_u32(header, 0x90, 0x02);
    set_u32(header, 0x94, 0x03);
    return header;
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
    if (result.magic != 6 && result.magic != 4 && result.magic != 3)
        throw std::runtime_error("unsupported texture magic " + std::to_string(result.magic));
    result.header_data_size = result.magic == 3 ? 0x8cU : result.magic == 6
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
    if (result.magic == 3) {
        const auto position = input.tellg();
        if (position < 0) throw std::runtime_error("could not locate opaque texture payload");
        result.individual_mipmap_payloads = false;
        result.payload_offset = static_cast<std::uint64_t>(position);
        if (file_size < result.payload_offset + 0x1cU) throw std::runtime_error("truncated opaque texture payload");
        result.payload_size = file_size - result.payload_offset - 0x1cU;
        result.payload_sha256 = sha256_range(input, result.payload_offset, result.payload_size);
        result.footer_size = 0x1cU;
        return result;
    }
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
    result.payload_offset = result.mipmaps.front().data_offset;
    const auto last_end = result.mipmaps.back().data_offset + result.mipmaps.back().data_size;
    result.payload_size = last_end - result.payload_offset;
    result.payload_sha256 = sha256_range(input, result.payload_offset, result.payload_size);
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
        if (source.individual_mipmap_payloads != reference.individual_mipmap_payloads)
            return {false, "payload layout differs"};
        if (source.individual_mipmap_payloads) {
            if (left.data_size != right.data_size) return {false, "mipmap payload size differs"};
            if (left.sha256 != right.sha256) return {false, "mipmap payload bytes differ"};
        }
    }
    if (!source.individual_mipmap_payloads &&
        (source.payload_size != reference.payload_size || source.payload_sha256 != reference.payload_sha256))
        return {false, "opaque aggregate payload differs"};
    return {true, source.individual_mipmap_payloads
        ? "all mipmap dimensions, flags, sizes, and payload hashes match"
        : "all mipmap metadata and the opaque aggregate payload hash match"};
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
               << ", flags=" << mip.flags;
        if (texture.individual_mipmap_payloads)
            output << ", size=" << mip.data_size << ", sha256=" << mip.sha256;
        output << '\n';
    }
    output << "Payload layout: " << (texture.individual_mipmap_payloads ? "individual mipmaps" : "opaque aggregate") << '\n'
           << "Payload offset: " << texture.payload_offset << '\n'
           << "Payload size: " << texture.payload_size << '\n'
           << "Payload SHA-256: " << texture.payload_sha256 << '\n';
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
            std::uint32_t bpp = 0;
            if (texture.individual_mipmap_payloads) {
                const auto pixels = static_cast<std::uint64_t>(first.width) * static_cast<std::uint64_t>(first.height);
                bpp = pixels == 0 ? 0U : static_cast<std::uint32_t>(first.data_size * 8ULL / pixels);
            } else {
                std::uint64_t pixels = 0;
                for (const auto& mip : texture.mipmaps)
                    pixels += static_cast<std::uint64_t>(mip.width) * static_cast<std::uint64_t>(mip.height);
                bpp = pixels == 0 ? 0U : static_cast<std::uint32_t>(
                    (texture.payload_size * 8ULL + pixels / 2ULL) / pixels);
            }
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

TextureConversionResult convert_linear_ps4_texture_to_pc(
    const std::filesystem::path& source, const std::filesystem::path& destination) {
    if (destination.empty() || std::filesystem::exists(destination))
        throw std::runtime_error("texture conversion destination is empty or exists");
    const auto parent = destination.has_parent_path() ? destination.parent_path() : std::filesystem::path{"."};
    if (!std::filesystem::is_directory(parent)) throw std::runtime_error("texture conversion directory does not exist");
    const auto texture = inspect_texture_container(source);
    if (texture.magic != 6 || texture.platform_version != 0x0c ||
        !texture.individual_mipmap_payloads || texture.mipmaps.size() != 7)
        throw std::runtime_error("conversion supports only linear magic-6/version-12 textures with seven mipmaps");
    std::uint32_t expected_dimension = 256;
    std::uint32_t bits_per_pixel = 0;
    for (const auto& mip : texture.mipmaps) {
        if (mip.width != static_cast<std::int32_t>(expected_dimension) ||
            mip.height != static_cast<std::int32_t>(expected_dimension) || mip.flags != 1)
            throw std::runtime_error("texture does not match the supported 256x256 square mipmap chain");
        const auto pixels = static_cast<std::uint64_t>(expected_dimension) * expected_dimension;
        const auto level_bpp = static_cast<std::uint32_t>(mip.data_size * 8ULL / pixels);
        if ((level_bpp != 4 && level_bpp != 8) || mip.data_size * 8ULL != pixels * level_bpp)
            throw std::runtime_error("texture mipmap is not a supported linear 4bpp or 8bpp payload");
        if (bits_per_pixel == 0) bits_per_pixel = level_bpp;
        if (level_bpp != bits_per_pixel) throw std::runtime_error("texture mipmap storage rate changes between levels");
        expected_dimension /= 2;
    }

    std::ifstream input(source, std::ios::binary);
    input.seekg(0, std::ios::end);
    const auto source_size = input.tellg();
    if (source_size < 28) throw std::runtime_error("texture source is truncated");
    input.seekg(0);
    std::vector<unsigned char> source_bytes(static_cast<std::size_t>(source_size));
    input.read(reinterpret_cast<char*>(source_bytes.data()), source_size);
    if (!input) throw std::runtime_error("could not read texture source");

    auto temporary = destination;
    temporary += ".rb4pc-part";
    auto manifest = destination;
    manifest += ".rb4pc-conversion.json";
    auto manifest_temporary = manifest;
    manifest_temporary += ".rb4pc-part";
    if (std::filesystem::exists(temporary) || std::filesystem::exists(manifest) ||
        std::filesystem::exists(manifest_temporary))
        throw std::runtime_error("texture conversion temporary or manifest path exists");
    TextureConversionResult result;
    result.destination = destination;
    result.manifest = manifest;
    result.bits_per_pixel = bits_per_pixel;
    result.source_sha256 = sha256_file(source);
    result.payload_sha256 = texture.payload_sha256;
    try {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        write_u32(output, 6);
        write_u32(output, 9);
        const auto header = canonical_pc_header(bits_per_pixel);
        output.write(reinterpret_cast<const char*>(header.data()), static_cast<std::streamsize>(header.size()));
        write_u32(output, static_cast<std::uint32_t>(texture.mipmaps.size()));
        for (const auto& mip : texture.mipmaps) {
            write_u32(output, static_cast<std::uint32_t>(mip.width));
            write_u32(output, static_cast<std::uint32_t>(mip.height));
            write_u32(output, static_cast<std::uint32_t>(mip.flags));
        }
        write_u32(output, 6);
        for (const auto& mip : texture.mipmaps) {
            write_u32(output, mip.data_size);
            output.write(reinterpret_cast<const char*>(source_bytes.data() + mip.data_offset), mip.data_size);
        }
        output.write(reinterpret_cast<const char*>(source_bytes.data() + source_bytes.size() - 28), 28);
        output.close();
        if (!output) throw std::runtime_error("could not write converted texture");
        std::filesystem::rename(temporary, destination);
        const auto converted = inspect_texture_container(destination);
        const auto comparison = compare_texture_payloads(texture, converted);
        if (converted.magic != 6 || converted.platform_version != 9 || converted.marker != 6 ||
            !comparison.compatible)
            throw std::runtime_error("converted texture failed structural or payload verification");
        result.destination_sha256 = sha256_file(destination);
        std::ofstream manifest_output(manifest_temporary, std::ios::binary | std::ios::trunc);
        if (!manifest_output) throw std::runtime_error("could not create texture conversion manifest");
        manifest_output << "{\n  \"schemaVersion\": 1,\n"
                        << "  \"conversion\": \"linear-ps4-v12-to-pc-v9\",\n"
                        << "  \"bitsPerPixel\": " << bits_per_pixel << ",\n"
                        << "  \"sourceSha256\": \"" << result.source_sha256 << "\",\n"
                        << "  \"destinationSha256\": \"" << result.destination_sha256 << "\",\n"
                        << "  \"payloadSha256\": \"" << result.payload_sha256 << "\"\n}\n";
        manifest_output.close();
        if (!manifest_output) throw std::runtime_error("could not finalize texture conversion manifest");
        std::filesystem::rename(manifest_temporary, manifest);
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        std::filesystem::remove(manifest_temporary, ignored);
        std::filesystem::remove(manifest, ignored);
        std::filesystem::remove(destination, ignored);
        throw;
    }
    return result;
}
} // namespace rb4pc::inspector
