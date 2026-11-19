#include "rb4pc/runtime/mogg_mix.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace rb4pc::runtime {
namespace {

struct Node {
    std::string atom;
    std::vector<Node> children;
    [[nodiscard]] bool is_list() const { return !children.empty(); }
};

std::vector<std::string> tokenize(std::istream& input) {
    std::vector<std::string> tokens;
    std::string token;
    bool comment = false;
    char value = 0;
    while (input.get(value)) {
        if (comment) {
            if (value == '\n') comment = false;
            continue;
        }
        if (value == ';') {
            if (!token.empty()) { tokens.push_back(std::move(token)); token.clear(); }
            comment = true;
        } else if (value == '(' || value == ')') {
            if (!token.empty()) { tokens.push_back(std::move(token)); token.clear(); }
            tokens.emplace_back(1, value);
        } else if (std::isspace(static_cast<unsigned char>(value))) {
            if (!token.empty()) { tokens.push_back(std::move(token)); token.clear(); }
        } else token.push_back(value);
    }
    if (!token.empty()) tokens.push_back(std::move(token));
    return tokens;
}

Node parse_node(const std::vector<std::string>& tokens, std::size_t& position) {
    if (position >= tokens.size()) throw std::runtime_error("unexpected end of MOGG mix metadata");
    if (tokens[position] != "(") return {tokens[position++], {}};
    ++position;
    Node result;
    while (position < tokens.size() && tokens[position] != ")")
        result.children.push_back(parse_node(tokens, position));
    if (position >= tokens.size()) throw std::runtime_error("unterminated MOGG mix metadata list");
    ++position;
    return result;
}

const Node* named_expression(const std::vector<Node>& roots, std::string_view name) {
    const auto found = std::find_if(roots.begin(), roots.end(), [name](const auto& node) {
        return node.children.size() >= 2 && node.children[0].atom == name;
    });
    return found == roots.end() ? nullptr : &*found;
}

float parse_float(const Node& node, const char* field) {
    try {
        std::size_t consumed = 0;
        const auto result = std::stof(node.atom, &consumed);
        if (consumed != node.atom.size() || !std::isfinite(result)) throw std::runtime_error("invalid");
        return result;
    } catch (...) { throw std::runtime_error(std::string("invalid ") + field + " value"); }
}

std::uint32_t parse_channel(const Node& node) {
    try {
        std::size_t consumed = 0;
        const auto result = std::stoul(node.atom, &consumed);
        if (consumed != node.atom.size() || result > std::numeric_limits<std::uint32_t>::max())
            throw std::runtime_error("invalid");
        return static_cast<std::uint32_t>(result);
    } catch (...) { throw std::runtime_error("invalid MOGG channel index"); }
}

} // namespace

std::size_t StereoPcmBlock::frame_count() const { return samples.size() / 2; }

MoggMixMetadata parse_mogg_mix_metadata(const std::filesystem::path& path,
                                        std::uint32_t expected_channels) {
    if (expected_channels == 0) throw std::runtime_error("MOGG mix expects at least one channel");
    std::ifstream input(path);
    if (!input) throw std::runtime_error("could not open MOGG mix metadata");
    const auto tokens = tokenize(input);
    std::vector<Node> roots;
    for (std::size_t position = 0; position < tokens.size();) roots.push_back(parse_node(tokens, position));
    const auto* tracks = named_expression(roots, "tracks");
    const auto* pans = named_expression(roots, "pans");
    const auto* volumes = named_expression(roots, "vols");
    if (tracks == nullptr || pans == nullptr || volumes == nullptr)
        throw std::runtime_error("MOGG mix metadata requires tracks, pans, and vols");
    if (!tracks->children[1].is_list()) throw std::runtime_error("invalid MOGG tracks list");

    MoggMixMetadata result;
    std::vector<bool> assigned(expected_channels, false);
    for (const auto& stem : tracks->children[1].children) {
        if (stem.children.size() != 2 || stem.children[0].atom.empty() || !stem.children[1].is_list())
            throw std::runtime_error("invalid MOGG stem entry");
        MoggStemChannels assignment{stem.children[0].atom, {}};
        for (const auto& channel_node : stem.children[1].children) {
            const auto channel = parse_channel(channel_node);
            if (channel >= expected_channels || assigned[channel])
                throw std::runtime_error("MOGG channel is out of range or assigned twice");
            assigned[channel] = true;
            assignment.channels.push_back(channel);
        }
        result.stems.push_back(std::move(assignment));
    }
    if (!std::all_of(assigned.begin(), assigned.end(), [](bool value) { return value; }))
        throw std::runtime_error("MOGG mix metadata leaves audio channels unassigned");
    if (pans->children.size() != 2 || volumes->children.size() != 2 ||
        !pans->children[1].is_list() || !volumes->children[1].is_list())
        throw std::runtime_error("invalid MOGG pan/volume lists");
    for (const auto& value : pans->children[1].children)
        result.pans.push_back(parse_float(value, "pan"));
    for (const auto& value : volumes->children[1].children)
        result.volumes_db.push_back(parse_float(value, "volume"));
    if (result.pans.size() != expected_channels || result.volumes_db.size() != expected_channels)
        throw std::runtime_error("MOGG pan/volume count does not match the audio channels");
    if (!std::all_of(result.pans.begin(), result.pans.end(), [](float pan) { return pan >= -1.0F && pan <= 1.0F; }))
        throw std::runtime_error("MOGG pan is outside -1..1");
    return result;
}

StereoPcmBlock mix_mogg_to_stereo(const VorbisPcmBlock& source,
                                  const MoggMixMetadata& metadata,
                                  const MoggMixConfig& config) {
    if (source.channels == 0 || source.samples.size() % source.channels != 0)
        throw std::runtime_error("invalid interleaved MOGG PCM block");
    if (metadata.pans.size() != source.channels || metadata.volumes_db.size() != source.channels)
        throw std::runtime_error("MOGG mix metadata does not match PCM channels");
    if (!std::isfinite(config.master_level) || config.master_level < 0.0F)
        throw std::runtime_error("invalid MOGG master level");
    std::vector<float> channel_levels(source.channels, 1.0F);
    for (const auto& stem : metadata.stems) {
        const auto configured = config.stem_levels.find(stem.name);
        const auto level = configured == config.stem_levels.end() ? 1.0F : configured->second;
        if (!std::isfinite(level) || level < 0.0F) throw std::runtime_error("invalid MOGG stem level");
        for (const auto channel : stem.channels) {
            if (channel >= source.channels) throw std::runtime_error("MOGG stem channel exceeds PCM channels");
            channel_levels[channel] = level;
        }
    }

    StereoPcmBlock result{source.first_frame, source.sample_rate, {}, 0.0F, 0.0F, 0};
    result.samples.resize(source.frame_count() * 2);
    for (std::size_t frame = 0; frame < source.frame_count(); ++frame) {
        float left = 0.0F, right = 0.0F;
        for (std::uint32_t channel = 0; channel < source.channels; ++channel) {
            const auto gain = std::pow(10.0F, metadata.volumes_db[channel] / 20.0F) *
                              channel_levels[channel] * config.master_level;
            const auto sample = source.samples[frame * source.channels + channel] * gain;
            const auto pan = metadata.pans[channel];
            left += sample * std::sqrt((1.0F - pan) * 0.5F);
            right += sample * std::sqrt((1.0F + pan) * 0.5F);
        }
        result.pre_clamp_peak_amplitude = std::max(
            {result.pre_clamp_peak_amplitude, std::abs(left), std::abs(right)});
        if (config.clamp_output) {
            if (std::abs(left) > 1.0F) ++result.clipped_samples;
            if (std::abs(right) > 1.0F) ++result.clipped_samples;
            left = std::clamp(left, -1.0F, 1.0F);
            right = std::clamp(right, -1.0F, 1.0F);
        }
        result.samples[frame * 2] = left;
        result.samples[frame * 2 + 1] = right;
        result.peak_amplitude = std::max({result.peak_amplitude, std::abs(left), std::abs(right)});
    }
    return result;
}

} // namespace rb4pc::runtime
