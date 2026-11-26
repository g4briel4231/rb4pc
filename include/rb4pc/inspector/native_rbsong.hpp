#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace rb4pc::inspector {

struct NativeRbsongHeader {
    std::uint32_t resource_version = 0;
    std::vector<std::string> inline_layer_names;
    std::optional<std::uint32_t> version_15_field;
    std::optional<std::uint32_t> version_16_field;
    std::uint32_t entity_version = 0;
    std::uint64_t entity_offset = 0;
};

struct NativeRbsongPropertySummary {
    std::string name;
    std::string type;
};

struct NativeRbsongComponentSummary {
    std::string name;
    std::string secondary_name;
    std::int32_t revision = 0;
    std::int64_t unknown = 0;
    std::vector<NativeRbsongPropertySummary> properties;
};

struct NativeRbsongObjectSummary {
    bool sentinel = false;
    std::int16_t layer = 0;
    std::int32_t index = 0;
    std::int32_t revision = 0;
    std::string name;
    std::vector<NativeRbsongComponentSummary> components;
};

struct NativeRbsongLayerSummary {
    std::int32_t version = 0;
    std::int32_t file_slot_index = 0;
    std::int16_t total_object_layers = 0;
    std::vector<NativeRbsongObjectSummary> objects;
};

struct NativeRbsongTreeSummary {
    NativeRbsongHeader header;
    std::vector<NativeRbsongLayerSummary> layers;
};

[[nodiscard]] NativeRbsongHeader read_native_rbsong_header(const std::filesystem::path& path);
[[nodiscard]] NativeRbsongTreeSummary read_native_rbsong_tree(const std::filesystem::path& path);
[[nodiscard]] std::string format_native_rbsong_header_json(const NativeRbsongHeader& header);
[[nodiscard]] std::string format_native_rbsong_tree_json(const NativeRbsongTreeSummary& tree);

} // namespace rb4pc::inspector