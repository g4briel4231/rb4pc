#include "rb4pc/runtime/midi_parser.hpp"
#include <algorithm>
#include <array>
#include <deque>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace rb4pc::runtime {
namespace {
std::uint32_t be32(const std::vector<unsigned char>& data, std::size_t p) {
    return (static_cast<std::uint32_t>(data[p]) << 24) | (static_cast<std::uint32_t>(data[p + 1]) << 16) |
           (static_cast<std::uint32_t>(data[p + 2]) << 8) | data[p + 3];
}
std::uint32_t vlq(const std::vector<unsigned char>& data, std::size_t& p, std::size_t end) {
    std::uint32_t value = 0;
    for (int count = 0; count < 4; ++count) {
        if (p >= end) throw std::runtime_error("truncated MIDI variable-length value");
        const auto byte = data[p++]; value = (value << 7) | (byte & 0x7f);
        if ((byte & 0x80) == 0) return value;
    }
    throw std::runtime_error("MIDI variable-length value exceeds four bytes");
}
}
MidiHeader parse_standard_midi_header(const std::filesystem::path& path) {
    if (!std::filesystem::is_regular_file(path)) throw std::runtime_error("MIDI path is not a file");
    std::ifstream input(path, std::ios::binary); std::array<unsigned char, 14> bytes{};
    input.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
    if (input.gcount() != static_cast<std::streamsize>(bytes.size()) ||
        std::string(reinterpret_cast<const char*>(bytes.data()), 4) != "MThd") throw std::runtime_error("not a standard MIDI file");
    const auto be16 = [&bytes](std::size_t p) { return static_cast<std::uint16_t>((bytes[p] << 8) | bytes[p + 1]); };
    const auto header_size = (static_cast<std::uint32_t>(bytes[4]) << 24) | (static_cast<std::uint32_t>(bytes[5]) << 16) |
                             (static_cast<std::uint32_t>(bytes[6]) << 8) | bytes[7];
    if (header_size != 6) throw std::runtime_error("unsupported MIDI header length");
    return {be16(8), be16(10), be16(12), std::filesystem::file_size(path)};
}

MidiFileSummary parse_standard_midi(const std::filesystem::path& path) {
    MidiFileSummary result; result.header = parse_standard_midi_header(path);
    std::ifstream input(path, std::ios::binary);
    std::vector<unsigned char> data(static_cast<std::size_t>(result.header.file_size));
    input.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
    std::size_t position = 14;
    for (std::uint16_t track_index = 0; track_index < result.header.track_count; ++track_index) {
        if (position + 8 > data.size() || std::string(reinterpret_cast<const char*>(data.data() + position), 4) != "MTrk")
            throw std::runtime_error("missing MIDI track chunk");
        const auto length = be32(data, position + 4); position += 8;
        if (length > data.size() - position) throw std::runtime_error("MIDI track exceeds file bounds");
        const auto end = position + length; MidiTrackSummary track; std::uint8_t running = 0; std::uint64_t tick = 0;
        struct ActiveNote { std::uint64_t start_tick; std::uint8_t velocity; };
        std::array<std::array<std::deque<ActiveNote>, 128>, 16> active_notes;
        while (position < end) {
            tick += vlq(data, position, end); if (position >= end) throw std::runtime_error("truncated MIDI event");
            auto status = data[position];
            if (status < 0x80) { if (running == 0) throw std::runtime_error("MIDI running status without status byte"); status = running; }
            else { ++position; if (status < 0xf0) running = status; }
            ++track.event_count;
            if (status == 0xff) {
                running = 0;
                if (position >= end) throw std::runtime_error("truncated MIDI meta event");
                const auto type = data[position++]; const auto size = vlq(data, position, end);
                if (size > end - position) throw std::runtime_error("MIDI meta event exceeds track");
                if (type == 0x03 && track.name.empty()) track.name.assign(reinterpret_cast<const char*>(data.data() + position), size);
                if (type == 0x51) {
                    if (size != 3) throw std::runtime_error("invalid MIDI tempo event length");
                    ++result.tempo_events;
                    const auto tempo = (static_cast<std::uint32_t>(data[position]) << 16) |
                                       (static_cast<std::uint32_t>(data[position + 1]) << 8) |
                                       data[position + 2];
                    if (tempo == 0) throw std::runtime_error("MIDI tempo cannot be zero");
                    result.tempo_changes.push_back({tick, tempo});
                }
                position += size;
            } else if (status == 0xf0 || status == 0xf7) {
                running = 0;
                const auto size = vlq(data, position, end); if (size > end - position) throw std::runtime_error("MIDI SysEx exceeds track"); position += size;
            } else {
                if (status >= 0xf0) throw std::runtime_error("unsupported MIDI system event");
                const auto kind = status & 0xf0; const auto bytes = (kind == 0xc0 || kind == 0xd0) ? 1U : 2U;
                if (bytes > end - position) throw std::runtime_error("MIDI channel event exceeds track");
                const auto first = data[position]; const auto second = bytes == 2 ? data[position + 1] : 0;
                if (first >= 0x80 || second >= 0x80) throw std::runtime_error("invalid MIDI channel data byte");
                const auto channel = status & 0x0f;
                if (kind == 0x90 && second != 0) {
                    ++track.note_on_count;
                    active_notes[channel][first].push_back({tick, static_cast<std::uint8_t>(second)});
                }
                if (kind == 0x80 || (kind == 0x90 && second == 0)) {
                    ++track.note_off_count;
                    auto& active = active_notes[channel][first];
                    if (active.empty()) ++track.unmatched_note_off_count;
                    else {
                        const auto started = active.front(); active.pop_front();
                        track.notes.push_back({static_cast<std::uint8_t>(channel), first, started.velocity, started.start_tick, tick});
                    }
                }
                position += bytes;
            }
        }
        for (const auto& channels : active_notes) for (const auto& notes : channels) track.dangling_note_on_count += notes.size();
        track.end_tick = tick; result.tracks.push_back(std::move(track)); position = end;
    }
    std::stable_sort(result.tempo_changes.begin(), result.tempo_changes.end(),
                     [](const auto& left, const auto& right) { return left.tick < right.tick; });
    return result;
}

double midi_tick_to_seconds(const MidiFileSummary& midi, std::uint64_t tick) {
    if ((midi.header.division & 0x8000U) != 0 || midi.header.division == 0)
        throw std::runtime_error("SMPTE or zero MIDI division is unsupported");
    std::uint64_t previous_tick = 0; std::uint32_t tempo = 500000; double seconds = 0.0;
    for (const auto& change : midi.tempo_changes) {
        if (change.tick > tick) break;
        if (change.tick > previous_tick) {
            seconds += static_cast<double>(change.tick - previous_tick) * tempo /
                       (static_cast<double>(midi.header.division) * 1'000'000.0);
        }
        previous_tick = change.tick; tempo = change.microseconds_per_quarter;
    }
    seconds += static_cast<double>(tick - previous_tick) * tempo /
               (static_cast<double>(midi.header.division) * 1'000'000.0);
    return seconds;
}
} // namespace rb4pc::runtime
