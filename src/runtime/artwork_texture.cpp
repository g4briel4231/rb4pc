#include "rb4pc/runtime/artwork_texture.hpp"

#include "rb4pc/inspector/texture_container.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <stdexcept>

namespace rb4pc::runtime {
namespace {
struct Color {
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;
    std::uint8_t alpha = 255;
};

std::uint16_t read_u16(const unsigned char* bytes) {
    return static_cast<std::uint16_t>(bytes[0]) |
           static_cast<std::uint16_t>(bytes[1]) << 8U;
}

Color rgb565(std::uint16_t value) {
    const auto red = static_cast<std::uint8_t>((value >> 11U) & 0x1fU);
    const auto green = static_cast<std::uint8_t>((value >> 5U) & 0x3fU);
    const auto blue = static_cast<std::uint8_t>(value & 0x1fU);
    return {static_cast<std::uint8_t>((red << 3U) | (red >> 2U)),
            static_cast<std::uint8_t>((green << 2U) | (green >> 4U)),
            static_cast<std::uint8_t>((blue << 3U) | (blue >> 2U)), 255};
}

Color interpolate(const Color& left, const Color& right, unsigned left_weight,
                  unsigned right_weight, unsigned divisor) {
    return {static_cast<std::uint8_t>((left.red * left_weight + right.red * right_weight) / divisor),
            static_cast<std::uint8_t>((left.green * left_weight + right.green * right_weight) / divisor),
            static_cast<std::uint8_t>((left.blue * left_weight + right.blue * right_weight) / divisor), 255};
}

std::array<Color, 4> color_table(const unsigned char* block, bool allow_transparency) {
    const auto color0 = read_u16(block);
    const auto color1 = read_u16(block + 2);
    std::array<Color, 4> colors{rgb565(color0), rgb565(color1), {}, {}};
    if (!allow_transparency || color0 > color1) {
        colors[2] = interpolate(colors[0], colors[1], 2, 1, 3);
        colors[3] = interpolate(colors[0], colors[1], 1, 2, 3);
    } else {
        colors[2] = interpolate(colors[0], colors[1], 1, 1, 2);
        colors[3] = {0, 0, 0, 0};
    }
    return colors;
}

void store_block(DecodedArtwork& output, std::uint32_t block_x, std::uint32_t block_y,
                 const std::array<Color, 16>& colors) {
    for (std::uint32_t pixel_y = 0; pixel_y < 4; ++pixel_y) {
        for (std::uint32_t pixel_x = 0; pixel_x < 4; ++pixel_x) {
            const auto x = block_x * 4 + pixel_x;
            const auto y = block_y * 4 + pixel_y;
            if (x >= output.width || y >= output.height) continue;
            const auto& color = colors[pixel_y * 4 + pixel_x];
            output.pixels[static_cast<std::size_t>(y) * output.width + x] =
                static_cast<std::uint32_t>(color.alpha) << 24U |
                static_cast<std::uint32_t>(color.red) << 16U |
                static_cast<std::uint32_t>(color.green) << 8U |
                color.blue;
        }
    }
}

void decode_bc1_block(DecodedArtwork& output, std::uint32_t block_x, std::uint32_t block_y,
                      const unsigned char* block) {
    const auto palette = color_table(block, true);
    const auto indices = static_cast<std::uint32_t>(block[4]) |
                         static_cast<std::uint32_t>(block[5]) << 8U |
                         static_cast<std::uint32_t>(block[6]) << 16U |
                         static_cast<std::uint32_t>(block[7]) << 24U;
    std::array<Color, 16> colors{};
    for (std::uint32_t index = 0; index < 16; ++index)
        colors[index] = palette[(indices >> (index * 2U)) & 3U];
    store_block(output, block_x, block_y, colors);
}

void decode_bc3_block(DecodedArtwork& output, std::uint32_t block_x, std::uint32_t block_y,
                      const unsigned char* block) {
    std::array<std::uint8_t, 8> alpha{block[0], block[1]};
    if (alpha[0] > alpha[1]) {
        for (unsigned index = 1; index <= 6; ++index)
            alpha[index + 1] = static_cast<std::uint8_t>(((7 - index) * alpha[0] + index * alpha[1]) / 7);
    } else {
        for (unsigned index = 1; index <= 4; ++index)
            alpha[index + 1] = static_cast<std::uint8_t>(((5 - index) * alpha[0] + index * alpha[1]) / 5);
        alpha[6] = 0;
        alpha[7] = 255;
    }
    std::uint64_t alpha_indices = 0;
    for (unsigned index = 0; index < 6; ++index)
        alpha_indices |= static_cast<std::uint64_t>(block[2 + index]) << (index * 8U);
    const auto palette = color_table(block + 8, false);
    const auto color_indices = static_cast<std::uint32_t>(block[12]) |
                               static_cast<std::uint32_t>(block[13]) << 8U |
                               static_cast<std::uint32_t>(block[14]) << 16U |
                               static_cast<std::uint32_t>(block[15]) << 24U;
    std::array<Color, 16> colors{};
    for (std::uint32_t index = 0; index < 16; ++index) {
        colors[index] = palette[(color_indices >> (index * 2U)) & 3U];
        colors[index].alpha = alpha[(alpha_indices >> (index * 3U)) & 7U];
    }
    store_block(output, block_x, block_y, colors);
}
} // namespace

DecodedArtwork decode_artwork_texture(const std::filesystem::path& path) {
    const auto texture = inspector::inspect_texture_container(path);
    if (texture.magic != 6 || texture.platform_version != 9 || texture.marker != 6 ||
        !texture.individual_mipmap_payloads || texture.mipmaps.empty())
        throw std::runtime_error("artwork is not a supported linear PC texture");
    const auto& mip = texture.mipmaps.front();
    const auto block_count = static_cast<std::uint64_t>((mip.width + 3) / 4) * ((mip.height + 3) / 4);
    const auto bc1_size = block_count * 8;
    const auto bc3_size = block_count * 16;
    if (mip.data_size != bc1_size && mip.data_size != bc3_size)
        throw std::runtime_error("artwork uses an unsupported texture compression");
    std::vector<unsigned char> payload(mip.data_size);
    std::ifstream input(path, std::ios::binary);
    input.seekg(static_cast<std::streamoff>(mip.data_offset));
    input.read(reinterpret_cast<char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
    if (!input) throw std::runtime_error("could not read artwork texture payload");
    DecodedArtwork output;
    output.width = static_cast<std::uint32_t>(mip.width);
    output.height = static_cast<std::uint32_t>(mip.height);
    output.pixels.resize(static_cast<std::size_t>(output.width) * output.height);
    const auto bytes_per_block = mip.data_size == bc1_size ? 8U : 16U;
    const auto blocks_wide = (output.width + 3) / 4;
    const auto blocks_high = (output.height + 3) / 4;
    for (std::uint32_t y = 0; y < blocks_high; ++y) {
        for (std::uint32_t x = 0; x < blocks_wide; ++x) {
            const auto* block = payload.data() + (static_cast<std::size_t>(y) * blocks_wide + x) * bytes_per_block;
            if (bytes_per_block == 8) decode_bc1_block(output, x, y, block);
            else decode_bc3_block(output, x, y, block);
        }
    }
    return output;
}

void render_artwork(SoftwareFramebuffer& framebuffer, const DecodedArtwork& artwork,
                    std::uint32_t x, std::uint32_t y,
                    std::uint32_t width, std::uint32_t height) {
    if (artwork.width == 0 || artwork.height == 0 || artwork.pixels.empty() || width == 0 || height == 0)
        return;
    const auto clipped_width = std::min(width, framebuffer.width() > x ? framebuffer.width() - x : 0U);
    const auto clipped_height = std::min(height, framebuffer.height() > y ? framebuffer.height() - y : 0U);
    for (std::uint32_t destination_y = 0; destination_y < clipped_height; ++destination_y) {
        const auto source_y = static_cast<std::uint32_t>(
            static_cast<std::uint64_t>(destination_y) * artwork.height / height);
        for (std::uint32_t destination_x = 0; destination_x < clipped_width; ++destination_x) {
            const auto source_x = static_cast<std::uint32_t>(
                static_cast<std::uint64_t>(destination_x) * artwork.width / width);
            framebuffer.set_pixel(x + destination_x, y + destination_y,
                                  artwork.pixels[static_cast<std::size_t>(source_y) * artwork.width + source_x]);
        }
    }
}

} // namespace rb4pc::runtime
