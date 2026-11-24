#include "rb4pc/builder/pc_archive_builder.hpp"

#include "rb4pc/builder/song_layout.hpp"
#include "rb4pc/inspector/file_inspector.hpp"
#include "rb4pc/inspector/murmur_hash3.hpp"
#include "rb4pc/inspector/sha256.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace rb4pc::builder {
namespace {

constexpr std::uint32_t hdr_version = 10;
constexpr std::uint32_t path_hash_seed = 0x9520e3c4;
constexpr std::uint32_t deterministic_hdr_seed = 0x52423450;
constexpr std::uint32_t sentinel = std::numeric_limits<std::uint32_t>::max();

struct SourceRecord {
    std::filesystem::path source;
    std::string internal_path;
    std::uint64_t offset = 0;
    std::uint32_t size = 0;
    std::uint32_t previous_hash_index = sentinel;
    std::size_t archive_index = 0;
};

void append_u32(std::vector<unsigned char>& bytes, std::uint32_t value) {
    for (unsigned int shift = 0; shift < 32; shift += 8)
        bytes.push_back(static_cast<unsigned char>(value >> shift));
}

void append_u64(std::vector<unsigned char>& bytes, std::uint64_t value) {
    append_u32(bytes, static_cast<std::uint32_t>(value));
    append_u32(bytes, static_cast<std::uint32_t>(value >> 32));
}

void append_string(std::vector<unsigned char>& bytes, std::string_view value) {
    if (value.size() > std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error("HDR string exceeds the format limit");
    append_u32(bytes, static_cast<std::uint32_t>(value.size()));
    bytes.insert(bytes.end(), value.begin(), value.end());
}

std::vector<unsigned char> encode_hdr(const std::vector<unsigned char>& plain) {
    std::vector<unsigned char> encoded;
    encoded.reserve(plain.size() + 4);
    append_u32(encoded, deterministic_hdr_seed);
    std::int64_t state = deterministic_hdr_seed;
    for (const auto value : plain) {
        const auto quotient = state / 127773;
        auto next = 16807 * (state % 127773) - 2836 * quotient;
        if (next <= 0) next += 2147483647;
        state = next;
        encoded.push_back(value ^ static_cast<unsigned char>(state & 0xff));
    }
    return encoded;
}

void copy_file(std::ofstream& output, const std::filesystem::path& source) {
    std::ifstream input(source, std::ios::binary);
    if (!input) throw std::runtime_error("could not open song payload: " + source.string());
    std::array<char, 64 * 1024> buffer{};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = input.gcount();
        if (count != 0) output.write(buffer.data(), count);
    }
    if (!input.eof() || !output) throw std::runtime_error("could not copy song payload into ARK");
}

} // namespace

PcArchiveBuildResult build_pc_song_archive(const std::filesystem::path& song_directory,
                                           const std::filesystem::path& output_directory,
                                           std::string_view short_name,
                                           PcArchiveBuildOptions options) {
    const auto package = custom_song_package_paths(short_name);
    if (options.maximum_archive_bytes == 0)
        throw std::runtime_error("maximum PC ARK size must be greater than zero");
    if (!std::filesystem::is_directory(song_directory))
        throw std::runtime_error("song source is not a directory");
    if (!std::filesystem::is_directory(output_directory))
        throw std::runtime_error("PC archive output directory does not exist");

    const auto header = output_directory / package.header;
    const auto header_temp = header.string() + ".tmp";
    if (std::filesystem::exists(header) || std::filesystem::exists(header_temp))
        throw std::runtime_error("PC archive destination already exists");

    std::vector<SourceRecord> records;
    const auto prefix = std::string("pc/songs_download/") + std::string(short_name) + '/';
    for (const auto& entry : std::filesystem::recursive_directory_iterator(song_directory)) {
        if (entry.is_symlink()) throw std::runtime_error("song source contains a symbolic link");
        if (!entry.is_regular_file()) continue;
        const auto relative = std::filesystem::relative(entry.path(), song_directory);
        const auto relative_name = relative.generic_string();
        if (relative_name.empty() || relative_name.starts_with("../"))
            throw std::runtime_error("song payload escapes the source directory");
        const auto size = entry.file_size();
        if (size > std::numeric_limits<std::uint32_t>::max())
            throw std::runtime_error("song payload exceeds the HDR record-size limit");
        records.push_back({entry.path(), prefix + relative_name, 0, static_cast<std::uint32_t>(size)});
    }
    if (records.empty()) throw std::runtime_error("song source contains no files");
    std::sort(records.begin(), records.end(), [](const auto& left, const auto& right) {
        return left.internal_path < right.internal_path;
    });
    if (records.size() > std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error("song source contains too many files");

    std::uint64_t payload_size = 0;
    std::vector<std::uint32_t> archive_sizes(1, 0);
    for (auto& record : records) {
        if (record.size > options.maximum_archive_bytes)
            throw std::runtime_error("song payload exceeds the configured PC ARK volume limit");
        if (archive_sizes.back() != 0 &&
            record.size > options.maximum_archive_bytes - archive_sizes.back())
            archive_sizes.push_back(0);
        record.archive_index = archive_sizes.size() - 1;
        record.offset = payload_size;
        payload_size += record.size;
        archive_sizes.back() += record.size;
    }

    std::vector<std::filesystem::path> archives;
    std::vector<std::filesystem::path> archive_temps;
    archives.reserve(archive_sizes.size());
    archive_temps.reserve(archive_sizes.size());
    for (std::size_t index = 0; index < archive_sizes.size(); ++index) {
        const auto name = std::string(short_name) + "_pc_" + std::to_string(index) + ".ark";
        const auto path = output_directory / name;
        const auto temporary = path.string() + ".tmp";
        if (std::filesystem::exists(path) || std::filesystem::exists(temporary))
            throw std::runtime_error("PC archive destination already exists");
        archives.push_back(path);
        archive_temps.emplace_back(temporary);
    }

    std::vector<std::uint32_t> buckets(records.size(), sentinel);
    for (std::uint32_t index = 0; index < records.size(); ++index) {
        const auto hash = inspector::murmur_hash3_x64_128(records[index].internal_path, path_hash_seed);
        const auto bucket = static_cast<std::uint32_t>(hash.low % records.size());
        records[index].previous_hash_index = buckets[bucket];
        buckets[bucket] = index;
    }

    try {
        for (std::size_t archive_index = 0; archive_index < archive_temps.size(); ++archive_index) {
            std::ofstream ark_output(archive_temps[archive_index], std::ios::binary);
            if (!ark_output) throw std::runtime_error("could not create temporary PC ARK");
            for (const auto& record : records)
                if (record.archive_index == archive_index) copy_file(ark_output, record.source);
            ark_output.close();
            if (!ark_output || std::filesystem::file_size(archive_temps[archive_index]) != archive_sizes[archive_index])
                throw std::runtime_error("PC ARK size verification failed");
        }

        std::vector<unsigned char> plain;
        append_u32(plain, hdr_version);
        append_u32(plain, 1);
        const std::array<unsigned char, 16> deterministic_guid = {
            'R','B','4','P','C','-','0','.','1','1','0','.','0',0,0,1};
        plain.insert(plain.end(), deterministic_guid.begin(), deterministic_guid.end());
        append_u32(plain, static_cast<std::uint32_t>(archives.size()));
        append_u32(plain, static_cast<std::uint32_t>(archive_sizes.size()));
        for (const auto size : archive_sizes) append_u32(plain, size);
        append_u32(plain, static_cast<std::uint32_t>(archives.size()));
        for (const auto& path : archives) append_string(plain, path.filename().generic_string());
        append_u32(plain, static_cast<std::uint32_t>(archives.size()));
        for (std::size_t index = 0; index < archives.size(); ++index) append_u32(plain, 0);
        append_u32(plain, static_cast<std::uint32_t>(records.size()));
        for (const auto& record : records) {
            append_u64(plain, record.offset);
            append_string(plain, record.internal_path);
            append_u32(plain, record.previous_hash_index);
            append_u32(plain, record.size);
        }
        append_u32(plain, static_cast<std::uint32_t>(buckets.size()));
        for (const auto head : buckets) append_u32(plain, head);

        const auto encoded = encode_hdr(plain);
        std::ofstream hdr_output(header_temp, std::ios::binary);
        if (!hdr_output) throw std::runtime_error("could not create temporary PC HDR");
        hdr_output.write(reinterpret_cast<const char*>(encoded.data()),
                         static_cast<std::streamsize>(encoded.size()));
        hdr_output.close();
        if (!hdr_output) throw std::runtime_error("could not finalize temporary PC HDR");

        const auto metadata = inspector::decode_hdr_prefix(header_temp);
        if (!metadata.hash_index_validated || metadata.archives.size() != archives.size() ||
            metadata.file_records.size() != records.size())
            throw std::runtime_error("generated PC HDR did not pass structural verification");
        for (std::size_t index = 0; index < archives.size(); ++index)
            if (metadata.archives[index].name != archives[index].filename().generic_string() ||
                metadata.archives[index].declared_size != archive_sizes[index])
                throw std::runtime_error("generated PC HDR archive table verification failed");

        for (std::size_t index = 0; index < archives.size(); ++index)
            std::filesystem::rename(archive_temps[index], archives[index]);
        std::filesystem::rename(header_temp, header);
        const auto report = inspector::inspect_header_set(header);
        if (report.archives.size() != archives.size() ||
            report.record_locations.size() != records.size() ||
            !std::all_of(report.record_locations.begin(), report.record_locations.end(),
                         [](const auto& location) { return location.has_value(); }))
            throw std::runtime_error("generated PC HDR/ARK set did not pass mount verification");
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(header_temp, ignored);
        std::filesystem::remove(header, ignored);
        for (const auto& path : archive_temps) std::filesystem::remove(path, ignored);
        for (const auto& path : archives) std::filesystem::remove(path, ignored);
        throw;
    }

    std::vector<std::string> archive_hashes;
    archive_hashes.reserve(archives.size());
    for (const auto& archive : archives) archive_hashes.push_back(inspector::sha256_file(archive));
    return {header, archives, records.size(), payload_size,
            inspector::sha256_file(header), std::move(archive_hashes)};
}

} // namespace rb4pc::builder
