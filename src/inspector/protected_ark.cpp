#include "rb4pc/inspector/protected_ark.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace rb4pc::inspector {
namespace {

std::uint16_t read_u16(const unsigned char* data) {
    return static_cast<std::uint16_t>(data[0]) |
           static_cast<std::uint16_t>(data[1]) << 8;
}

std::uint32_t read_u32(const unsigned char* data) {
    return static_cast<std::uint32_t>(data[0]) |
           static_cast<std::uint32_t>(data[1]) << 8 |
           static_cast<std::uint32_t>(data[2]) << 16 |
           static_cast<std::uint32_t>(data[3]) << 24;
}

std::uint32_t mangle(const std::vector<unsigned char>& data, std::size_t offset, std::size_t count) {
    std::uint32_t result = 0;
    for (std::size_t index = 0; index < count; ++index) {
        const auto value = static_cast<std::uint32_t>(data.at(offset + index));
        result = value ^ (2U * (value + result));
    }
    return result;
}

std::uint8_t fold(std::uint32_t value) {
    return static_cast<std::uint8_t>(value) +
           static_cast<std::uint8_t>(value >> 8) +
           static_cast<std::uint8_t>(value >> 16) +
           static_cast<std::uint8_t>(value >> 24);
}

std::uint32_t rotate_left(std::uint32_t value, int amount) {
    amount &= 31;
    return (value << amount) | (value >> ((32 - amount) & 31));
}

void hash_region(std::vector<unsigned char>& data, std::size_t offset, std::size_t count) {
    std::uint8_t counter = 0;
    std::uint32_t state = 0xe3afec21U;
    for (std::size_t index = 0; index < count; ++index) {
        const auto value = static_cast<std::uint8_t>(data.at(offset + index) ^ fold(state));
        data[offset + index] = value;
        auto repeated = static_cast<std::uint32_t>(value);
        repeated |= repeated << 8;
        repeated |= repeated << 8;
        repeated |= repeated << 8;
        state = rotate_left(repeated + rotate_left(state, value & 31), 1);
        if (counter > 16) {
            state *= 2;
            counter = 0;
        }
        ++counter;
    }
}

std::uint8_t calculate_initial_key(const std::vector<unsigned char>& source_material) {
    auto material = source_material;
    if (material.size() < 24) throw std::runtime_error("protected ARK key material is truncated");
    const auto variable_count = static_cast<std::size_t>(read_u16(material.data() + 14));
    if (variable_count > material.size() - 24) {
        throw std::runtime_error("protected ARK key material has an invalid variable region");
    }
    std::uint32_t value = mangle(material, 4, 9);
    value += mangle(material, 0, 4);
    value += mangle(material, 13, 1);
    value += mangle(material, 16, 4);
    value += mangle(material, 24, variable_count);
    const auto folded = fold(value);
    hash_region(material, 24, variable_count);
    hash_region(material, 13, 1);
    hash_region(material, 16, 4);
    hash_region(material, 0, 4);
    hash_region(material, 4, 9);
    return static_cast<std::uint8_t>(material[5] ^ folded);
}

void seek(std::ifstream& input, std::uint64_t position, const char* error) {
    if (position > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max())) {
        throw std::runtime_error(error);
    }
    input.seekg(static_cast<std::streamoff>(position));
    if (!input) throw std::runtime_error(error);
}

} // namespace

std::optional<ProtectedArkInfo> inspect_protected_ark(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("could not open ARK volume: " + path.string());
    input.seekg(0, std::ios::end);
    const auto end = input.tellg();
    if (end < 36) return std::nullopt;
    const auto stored_size = static_cast<std::uint64_t>(end);

    seek(input, stored_size - 36, "could not seek to protected ARK descriptor");
    std::array<unsigned char, 4> encoded_size{};
    input.read(reinterpret_cast<char*>(encoded_size.data()), encoded_size.size());
    if (input.gcount() != static_cast<std::streamsize>(encoded_size.size())) return std::nullopt;
    const auto material_size = read_u32(encoded_size.data());

    // The Harmonix wrapper stores the key-material length 36 bytes from EOF. After
    // reading it, the reference stream seeks backwards by that length and reads the
    // material, leaving a 32-byte tail outside that material.
    if (material_size < 24 || material_size > 1024U * 1024U ||
        static_cast<std::uint64_t>(material_size) > stored_size) {
        return std::nullopt;
    }
    const auto material_offset = stored_size - material_size;
    std::vector<unsigned char> material(material_size);
    seek(input, material_offset, "could not seek to protected ARK key material");
    input.read(reinterpret_cast<char*>(material.data()), static_cast<std::streamsize>(material.size()));
    if (static_cast<std::size_t>(input.gcount()) != material.size()) return std::nullopt;

    try {
        return ProtectedArkInfo{stored_size, stored_size - material_size, material_size,
                                calculate_initial_key(material)};
    } catch (const std::out_of_range&) {
        return std::nullopt;
    } catch (const std::runtime_error&) {
        return std::nullopt;
    }
}

void copy_ark_range(const std::filesystem::path& path,
                    std::uint64_t logical_offset,
                    std::uint64_t size,
                    std::ostream& output) {
    const auto protected_info = inspect_protected_ark(path);
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("could not open ARK volume: " + path.string());
    input.seekg(0, std::ios::end);
    const auto end = input.tellg();
    if (end < 0) throw std::runtime_error("could not determine ARK volume size");
    const auto logical_size = protected_info ? protected_info->logical_size : static_cast<std::uint64_t>(end);
    if (logical_offset > logical_size || size > logical_size - logical_offset) {
        throw std::runtime_error("requested range exceeds the logical ARK size");
    }

    std::uint8_t key = 0;
    if (protected_info) {
        if (logical_offset == 0) {
            key = protected_info->initial_key;
        } else {
            seek(input, logical_offset - 1, "could not seek to protected ARK predecessor byte");
            const auto predecessor = input.get();
            if (predecessor == std::char_traits<char>::eof()) {
                throw std::runtime_error("could not read protected ARK predecessor byte");
            }
            key = static_cast<std::uint8_t>((protected_info->initial_key ^
                                             static_cast<std::uint8_t>(predecessor)) -
                                            logical_offset + 1U);
        }
    }

    seek(input, logical_offset, "could not seek to requested ARK range");
    std::array<unsigned char, 64 * 1024> buffer{};
    std::uint64_t position = logical_offset;
    std::uint64_t remaining = size;
    while (remaining != 0) {
        const auto chunk = static_cast<std::size_t>(std::min<std::uint64_t>(remaining, buffer.size()));
        input.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(chunk));
        if (static_cast<std::size_t>(input.gcount()) != chunk) {
            throw std::runtime_error("unexpected end of ARK while reading logical range");
        }
        if (protected_info) {
            for (std::size_t index = 0; index < chunk; ++index) {
                const auto stored = buffer[index];
                buffer[index] ^= key;
                key = static_cast<std::uint8_t>((protected_info->initial_key ^ stored) - position);
                ++position;
            }
        }
        output.write(reinterpret_cast<const char*>(buffer.data()), static_cast<std::streamsize>(chunk));
        if (!output) throw std::runtime_error("could not write logical ARK range");
        remaining -= chunk;
    }
}

} // namespace rb4pc::inspector