#include "rb4pc/inspector/hdr_decoder.hpp"
#include "rb4pc/inspector/murmur_hash3.hpp"

#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace rb4pc::inspector {
namespace {

constexpr std::uint32_t maximum_archive_count = 4096;
constexpr std::uint32_t maximum_name_length = 4096;
constexpr std::uint32_t maximum_tags_per_archive = 4096;
constexpr std::uint32_t maximum_file_records = 10'000'000;
constexpr std::uint32_t path_hash_seed = 0x9520e3c4;

class DecodedReader {
public:
    explicit DecodedReader(const std::filesystem::path& path) : input_(path, std::ios::binary) {
        if (!input_) throw std::runtime_error("could not open HDR: " + path.string());
        std::array<unsigned char, 4> seed_bytes{};
        read_raw(seed_bytes.data(), seed_bytes.size());
        raw_seed_ = static_cast<std::uint32_t>(seed_bytes[0]) |
                    (static_cast<std::uint32_t>(seed_bytes[1]) << 8) |
                    (static_cast<std::uint32_t>(seed_bytes[2]) << 16) |
                    (static_cast<std::uint32_t>(seed_bytes[3]) << 24);
        const auto signed_seed = static_cast<std::int32_t>(raw_seed_);
        if (signed_seed == 0) state_ = 1;
        else if (signed_seed < 0) state_ = -static_cast<std::int64_t>(signed_seed);
        else state_ = signed_seed;
    }

    [[nodiscard]] std::uint32_t raw_seed() const { return raw_seed_; }

    void read(unsigned char* destination, std::size_t size) {
        read_raw(destination, size);
        for (std::size_t index = 0; index < size; ++index) {
            destination[index] ^= next_byte();
        }
        position_ += size;
    }

    [[nodiscard]] std::uint32_t read_u32() {
        std::array<unsigned char, 4> bytes{};
        read(bytes.data(), bytes.size());
        return static_cast<std::uint32_t>(bytes[0]) |
               (static_cast<std::uint32_t>(bytes[1]) << 8) |
               (static_cast<std::uint32_t>(bytes[2]) << 16) |
               (static_cast<std::uint32_t>(bytes[3]) << 24);
    }

    [[nodiscard]] std::uint64_t read_u64() {
        const auto low = static_cast<std::uint64_t>(read_u32());
        const auto high = static_cast<std::uint64_t>(read_u32());
        return low | (high << 32);
    }

    [[nodiscard]] std::string read_string() {
        const auto size = read_u32();
        if (size > maximum_name_length) throw std::runtime_error("HDR string length exceeds safety limit");
        std::string value(size, '\0');
        read(reinterpret_cast<unsigned char*>(value.data()), value.size());
        return value;
    }

    [[nodiscard]] std::uint64_t position() const { return position_; }

    [[nodiscard]] bool has_remaining_bytes() { return input_.peek() != std::char_traits<char>::eof(); }

private:
    void read_raw(unsigned char* destination, std::size_t size) {
        input_.read(reinterpret_cast<char*>(destination), static_cast<std::streamsize>(size));
        if (static_cast<std::size_t>(input_.gcount()) != size) {
            throw std::runtime_error("unexpected end of HDR while decoding prefix");
        }
    }

    [[nodiscard]] unsigned char next_byte() {
        const auto quotient = state_ / 127773;
        auto next = 16807 * (state_ % 127773) - 2836 * quotient;
        if (next <= 0) next += 2147483647;
        state_ = next;
        return static_cast<unsigned char>(state_ & 0xff);
    }

    std::ifstream input_;
    std::uint32_t raw_seed_ = 0;
    std::int64_t state_ = 1;
    std::uint64_t position_ = 0;
};

} // namespace

HdrMetadata decode_hdr_prefix(const std::filesystem::path& path) {
    DecodedReader reader(path);
    HdrMetadata metadata;
    metadata.raw_seed = reader.raw_seed();
    metadata.version = reader.read_u32();
    metadata.unknown_04 = reader.read_u32();
    reader.read(metadata.unknown_08.data(), metadata.unknown_08.size());

    const auto archive_count = reader.read_u32();
    const auto size_count = reader.read_u32();
    if (archive_count > maximum_archive_count || size_count > maximum_archive_count) {
        throw std::runtime_error("HDR archive count exceeds safety limit");
    }
    if (archive_count != size_count) throw std::runtime_error("HDR archive and size counts differ");

    std::vector<std::uint32_t> sizes;
    sizes.reserve(size_count);
    for (std::uint32_t index = 0; index < size_count; ++index) sizes.push_back(reader.read_u32());

    const auto name_count = reader.read_u32();
    if (name_count != archive_count) throw std::runtime_error("HDR archive and name counts differ");
    metadata.archives.reserve(archive_count);
    for (std::uint32_t index = 0; index < archive_count; ++index) {
        metadata.archives.push_back({reader.read_string(), sizes[index], {}});
    }

    const auto tag_set_count = reader.read_u32();
    if (tag_set_count != archive_count) throw std::runtime_error("HDR archive and tag-set counts differ");
    for (auto& archive : metadata.archives) {
        const auto tag_count = reader.read_u32();
        if (tag_count > maximum_tags_per_archive) throw std::runtime_error("HDR tag count exceeds safety limit");
        archive.tags.reserve(tag_count);
        for (std::uint32_t index = 0; index < tag_count; ++index) archive.tags.push_back(reader.read_string());
    }

    const auto record_count = reader.read_u32();
    if (record_count > maximum_file_records) throw std::runtime_error("HDR file-record count exceeds safety limit");
    metadata.file_records.reserve(record_count);
    for (std::uint32_t index = 0; index < record_count; ++index) {
        HdrFileRecord record;
        record.global_offset = reader.read_u64();
        record.path = reader.read_string();
        record.previous_hash_index = reader.read_u32();
        record.size = reader.read_u32();
        metadata.file_records.push_back(std::move(record));
    }


    const auto bucket_count = reader.read_u32();
    if (bucket_count != record_count) throw std::runtime_error("HDR hash-bucket and file-record counts differ");
    metadata.hash_bucket_heads.reserve(bucket_count);
    for (std::uint32_t index = 0; index < bucket_count; ++index) {
        const auto head = reader.read_u32();
        if (head != std::numeric_limits<std::uint32_t>::max() && head >= record_count) {
            throw std::runtime_error("HDR hash-bucket head is outside the file-record table");
        }
        metadata.hash_bucket_heads.push_back(head);
    }
    for (std::uint32_t index = 0; index < record_count; ++index) {
        const auto previous = metadata.file_records[index].previous_hash_index;
        if (previous != std::numeric_limits<std::uint32_t>::max() && previous >= index) {
            throw std::runtime_error("HDR hash-chain link does not reference an earlier record");
        }
    }
    std::vector<std::uint32_t> reconstructed_heads(record_count, std::numeric_limits<std::uint32_t>::max());
    for (std::uint32_t index = 0; index < record_count; ++index) {
        const auto hash = murmur_hash3_x64_128(metadata.file_records[index].path, path_hash_seed);
        const auto bucket = static_cast<std::uint32_t>(hash.low % record_count);
        if (metadata.file_records[index].previous_hash_index != reconstructed_heads[bucket]) {
            throw std::runtime_error("HDR path hash-chain link does not match the reconstructed index");
        }
        reconstructed_heads[bucket] = index;
    }
    if (reconstructed_heads != metadata.hash_bucket_heads) {
        throw std::runtime_error("HDR path hash-bucket table does not match the reconstructed index");
    }
    metadata.hash_index_validated = true;
    metadata.decoded_payload_size = reader.position();
    if (reader.has_remaining_bytes()) throw std::runtime_error("HDR contains unparsed trailing data");
    return metadata;
}

std::string hex_string(const unsigned char* data, std::size_t size) {
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (std::size_t index = 0; index < size; ++index) output << std::setw(2) << static_cast<unsigned int>(data[index]);
    return output.str();
}

} // namespace rb4pc::inspector
