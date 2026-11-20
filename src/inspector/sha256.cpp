#include "rb4pc/inspector/sha256.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace rb4pc::inspector {
namespace {

constexpr std::array<std::uint32_t, 64> constants{
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

class Sha256 {
public:
    void update(const unsigned char* data, std::size_t size) {
        for (std::size_t index = 0; index < size; ++index) {
            buffer_[buffer_size_++] = data[index];
            if (buffer_size_ == buffer_.size()) {
                transform();
                bit_count_ += 512;
                buffer_size_ = 0;
            }
        }
    }

    [[nodiscard]] std::array<std::uint32_t, 8> finish() {
        bit_count_ += static_cast<std::uint64_t>(buffer_size_) * 8;
        buffer_[buffer_size_++] = 0x80;

        if (buffer_size_ > 56) {
            while (buffer_size_ < 64) buffer_[buffer_size_++] = 0;
            transform();
            buffer_size_ = 0;
        }
        while (buffer_size_ < 56) buffer_[buffer_size_++] = 0;
        for (int shift = 56; shift >= 0; shift -= 8) {
            buffer_[buffer_size_++] = static_cast<unsigned char>(bit_count_ >> shift);
        }
        transform();
        return state_;
    }

private:
    void transform() {
        std::array<std::uint32_t, 64> words{};
        for (std::size_t i = 0; i < 16; ++i) {
            const auto offset = i * 4;
            words[i] = (static_cast<std::uint32_t>(buffer_[offset]) << 24) |
                       (static_cast<std::uint32_t>(buffer_[offset + 1]) << 16) |
                       (static_cast<std::uint32_t>(buffer_[offset + 2]) << 8) |
                       static_cast<std::uint32_t>(buffer_[offset + 3]);
        }
        for (std::size_t i = 16; i < 64; ++i) {
            const auto s0 = std::rotr(words[i - 15], 7) ^ std::rotr(words[i - 15], 18) ^ (words[i - 15] >> 3);
            const auto s1 = std::rotr(words[i - 2], 17) ^ std::rotr(words[i - 2], 19) ^ (words[i - 2] >> 10);
            words[i] = words[i - 16] + s0 + words[i - 7] + s1;
        }

        auto working = state_;
        for (std::size_t i = 0; i < 64; ++i) {
            const auto sum1 = std::rotr(working[4], 6) ^ std::rotr(working[4], 11) ^ std::rotr(working[4], 25);
            const auto choose = (working[4] & working[5]) ^ (~working[4] & working[6]);
            const auto temp1 = working[7] + sum1 + choose + constants[i] + words[i];
            const auto sum0 = std::rotr(working[0], 2) ^ std::rotr(working[0], 13) ^ std::rotr(working[0], 22);
            const auto majority = (working[0] & working[1]) ^ (working[0] & working[2]) ^ (working[1] & working[2]);
            const auto temp2 = sum0 + majority;
            working = {temp1 + temp2, working[0], working[1], working[2], working[3] + temp1, working[4], working[5], working[6]};
        }
        for (std::size_t i = 0; i < state_.size(); ++i) state_[i] += working[i];
    }

    std::array<std::uint32_t, 8> state_{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    std::array<unsigned char, 64> buffer_{};
    std::size_t buffer_size_ = 0;
    std::uint64_t bit_count_ = 0;
};

} // namespace

std::string sha256_range(std::istream& input, std::uint64_t offset, std::uint64_t size) {
    input.clear();
    input.seekg(static_cast<std::streamoff>(offset));
    if (!input) throw std::runtime_error("could not seek to hash range");
    Sha256 hash;
    std::array<unsigned char, 64 * 1024> chunk{};
    while (size != 0) {
        const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(size, chunk.size()));
        input.read(reinterpret_cast<char*>(chunk.data()), static_cast<std::streamsize>(count));
        if (static_cast<std::size_t>(input.gcount()) != count)
            throw std::runtime_error("unexpected end of hash range");
        hash.update(chunk.data(), count);
        size -= count;
    }
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const auto word : hash.finish()) output << std::setw(8) << word;
    return output.str();
}

std::string sha256_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("could not open file for hashing: " + path.string());

    input.seekg(0, std::ios::end);
    const auto size = input.tellg();
    if (size < 0) throw std::runtime_error("could not determine file size for hashing");
    return sha256_range(input, 0, static_cast<std::uint64_t>(size));
}

} // namespace rb4pc::inspector
