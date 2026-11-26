#include "rb4pc/inspector/native_rbsong.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace rb4pc::inspector {
namespace {

std::uint32_t read_u32(std::istream& input, const char* field) {
    unsigned char bytes[4]{};
    input.read(reinterpret_cast<char*>(bytes), 4);
    if (input.gcount() != 4) throw std::runtime_error(std::string("rbsong ended while reading ") + field);
    return static_cast<std::uint32_t>(bytes[0]) |
           static_cast<std::uint32_t>(bytes[1]) << 8 |
           static_cast<std::uint32_t>(bytes[2]) << 16 |
           static_cast<std::uint32_t>(bytes[3]) << 24;
}

std::string read_string(std::istream& input) {
    const auto size = read_u32(input, "inline layer name length");
    if (size > 1024U * 1024U) throw std::runtime_error("rbsong inline layer name is unreasonably large");
    std::string result(size, '\0');
    input.read(result.data(), static_cast<std::streamsize>(size));
    if (static_cast<std::uint32_t>(input.gcount()) != size) {
        throw std::runtime_error("rbsong ended while reading an inline layer name");
    }
    return result;
}

std::string escape_json(std::string_view value) {
    std::ostringstream output;
    for (const unsigned char character : value) {
        if (character == '"' || character == '\\') output << '\\' << static_cast<char>(character);
        else if (character < 0x20) {
            output << "\\u00" << "0123456789abcdef"[character >> 4]
                   << "0123456789abcdef"[character & 0x0f];
        } else output << static_cast<char>(character);
    }
    return output.str();
}

} // namespace

NativeRbsongHeader read_native_rbsong_header(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("could not open rbsong: " + path.string());
    NativeRbsongHeader result;
    result.resource_version = read_u32(input, "resource version");
    if (result.resource_version < 12 || result.resource_version > 17) {
        throw std::runtime_error("unsupported rbsong entity-resource version");
    }
    const auto layer_count = read_u32(input, "inline layer name count");
    if (layer_count > 1024) throw std::runtime_error("rbsong inline layer name count is unreasonably large");
    result.inline_layer_names.reserve(layer_count);
    for (std::uint32_t index = 0; index < layer_count; ++index) {
        result.inline_layer_names.push_back(read_string(input));
    }
    if (result.resource_version >= 15) result.version_15_field = read_u32(input, "version-15 field");
    if (result.resource_version >= 16) result.version_16_field = read_u32(input, "version-16 field");
    const auto entity_position = input.tellg();
    if (entity_position < 0) throw std::runtime_error("could not locate rbsong entity payload");
    result.entity_offset = static_cast<std::uint64_t>(entity_position);
    result.entity_version = read_u32(input, "entity version");
    return result;
}

namespace {

std::uint8_t read_byte(std::istream& input, const char* field) {
    const auto value = input.get();
    if (value == std::char_traits<char>::eof()) throw std::runtime_error(std::string("rbsong ended while reading ") + field);
    return static_cast<std::uint8_t>(value);
}

std::int16_t read_i16(std::istream& input, const char* field) {
    const auto low = read_byte(input, field);
    const auto high = read_byte(input, field);
    return static_cast<std::int16_t>(static_cast<std::uint16_t>(low) | static_cast<std::uint16_t>(high) << 8);
}

std::int32_t read_i32(std::istream& input, const char* field) {
    return static_cast<std::int32_t>(read_u32(input, field));
}

std::int64_t read_i64(std::istream& input, const char* field) {
    const auto low = static_cast<std::uint64_t>(read_u32(input, field));
    const auto high = static_cast<std::uint64_t>(read_u32(input, field));
    return static_cast<std::int64_t>(low | high << 32);
}

struct TypeDescription {
    std::int32_t code = 0;
    std::string name;
    std::vector<TypeDescription> children;
};

TypeDescription read_type(std::istream& input);

std::vector<NativeRbsongPropertySummary> read_property_definitions(
    std::istream& input, std::vector<TypeDescription>& types) {
    const auto count = read_u32(input, "property definition count");
    if (count > 100000) throw std::runtime_error("rbsong property definition count is unreasonably large");
    std::vector<NativeRbsongPropertySummary> properties;
    properties.reserve(count);
    types.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index) {
        auto name = read_string(input);
        auto type = read_type(input);
        properties.push_back({std::move(name), type.name});
        types.push_back(std::move(type));
    }
    return properties;
}

std::string primitive_type_name(std::int32_t code) {
    switch (code) {
    case 0: return "float";
    case 1: return "int8";
    case 2: return "int16";
    case 3: return "int32";
    case 4: return "int64";
    case 5: return "uint8";
    case 6: return "uint16";
    case 7: return "uint32";
    case 8: return "uint64";
    case 9: return "bool";
    case 10: return "game_object_id";
    case 11: return "symbol";
    case 12: return "resource_path";
    case 13: return "color";
    case 16: return "driven_prop";
    default: return "unknown(" + std::to_string(code) + ')';
    }
}

TypeDescription read_type(std::istream& input) {
    TypeDescription result;
    result.code = read_i32(input, "property type");
    if ((result.code & 256) == 256) {
        result.children.push_back(read_type(input));
        result.name = "array<" + result.children[0].name + '>';
        return result;
    }
    if ((result.code & 15) == 15) {
        static_cast<void>(read_i64(input, "struct reference count"));
        std::vector<TypeDescription> member_types;
        const auto members = read_property_definitions(input, member_types);
        result.name = "struct{";
        for (std::size_t index = 0; index < members.size(); ++index) {
            if (index != 0) result.name += ',';
            result.name += members[index].name + ':' + members[index].type;
        }
        result.name += '}';
        result.children = std::move(member_types);
        return result;
    }
    result.name = primitive_type_name(result.code);
    return result;
}

void skip_value(std::istream& input, const TypeDescription& type) {
    if ((type.code & 256) == 256) {
        const auto count = read_u32(input, "array value count");
        if (count > 1000000) throw std::runtime_error("rbsong array value count is unreasonably large");
        for (std::uint32_t index = 0; index < count; ++index) skip_value(input, type.children.at(0));
        return;
    }
    if ((type.code & 15) == 15) {
        for (const auto& child : type.children) skip_value(input, child);
        return;
    }
    switch (type.code) {
    case 0: static_cast<void>(read_u32(input, "float value")); return;
    case 3: static_cast<void>(read_i32(input, "int value")); return;
    case 5: static_cast<void>(read_byte(input, "byte value")); return;
    case 7: static_cast<void>(read_u32(input, "unsigned value")); return;
    case 8: static_cast<void>(read_i64(input, "long value")); return;
    case 9: static_cast<void>(read_byte(input, "boolean value")); return;
    case 10:
        for (int index = 0; index < 6; ++index) static_cast<void>(read_i32(input, "game object id value"));
        return;
    case 11: static_cast<void>(read_string(input)); return;
    case 12: static_cast<void>(read_byte(input, "resource path prefix")); static_cast<void>(read_string(input)); return;
    case 13:
        for (int index = 0; index < 8; ++index) static_cast<void>(read_u32(input, "color value"));
        return;
    case 16: {
        static_cast<void>(read_i32(input, "driven property field"));
        const auto mode = read_i32(input, "driven property mode");
        if (mode == 0) static_cast<void>(read_string(input));
        static_cast<void>(read_i32(input, "driven property field"));
        static_cast<void>(read_i64(input, "driven property field"));
        if (mode == 0) static_cast<void>(read_string(input));
        return;
    }
    default: throw std::runtime_error("unsupported rbsong value type " + std::to_string(type.code));
    }
}

NativeRbsongComponentSummary read_component(std::istream& input, std::int32_t object_revision,
                                             std::int32_t layer_version) {
    NativeRbsongComponentSummary component;
    component.name = read_string(input);
    if (object_revision >= 2) component.secondary_name = read_string(input);
    component.revision = read_i32(input, "component revision");
    component.unknown = read_i64(input, "component field");
    if (layer_version >= 14) {
        std::vector<TypeDescription> types;
        component.properties = read_property_definitions(input, types);
        for (const auto& type : types) skip_value(input, type);
    }
    return component;
}

NativeRbsongObjectSummary read_object(std::istream& input, std::int32_t layer_version) {
    NativeRbsongObjectSummary object;
    const auto packed_id = read_i32(input, "game object id");
    if (packed_id == -1) { object.sentinel = true; return object; }
    object.index = packed_id & 4095;
    object.layer = static_cast<std::int16_t>(packed_id >> 16);
    object.revision = read_i32(input, "game object revision");
    if (object.revision < 0) object.name.assign(1, static_cast<char>(read_byte(input, "legacy object name")));
    else object.name = read_string(input);
    if (object.revision >= 3 && object.name.empty()) {
        static_cast<void>(read_i32(input, "empty object field"));
        static_cast<void>(read_i32(input, "empty object field"));
        if (object.revision >= 4) static_cast<void>(read_i32(input, "empty object field"));
        return object;
    }
    const auto component_count = read_u32(input, "component count");
    if (component_count > 100000) throw std::runtime_error("rbsong component count is unreasonably large");
    object.components.reserve(component_count);
    for (std::uint32_t index = 0; index < component_count; ++index) {
        object.components.push_back(read_component(input, object.revision, layer_version));
    }
    return object;
}

} // namespace

NativeRbsongTreeSummary read_native_rbsong_tree(const std::filesystem::path& path) {
    NativeRbsongTreeSummary result;
    result.header = read_native_rbsong_header(path);
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("could not open rbsong: " + path.string());
    input.seekg(static_cast<std::streamoff>(result.header.entity_offset));
    if (!input) throw std::runtime_error("could not seek to rbsong entity payload");
    const auto entity_version = read_i32(input, "entity version");
    if (entity_version != static_cast<std::int32_t>(result.header.entity_version)) {
        throw std::runtime_error("rbsong entity version changed while parsing");
    }
    if (entity_version < 12 || entity_version > 30) {
        throw std::runtime_error("unsupported rbsong entity version");
    }
    const auto layer_count = read_u32(input, "entity layer count");
    if (layer_count != 1) {
        throw std::runtime_error("only the verified single-layer rbsong entity layout is supported");
    }
    result.layers.reserve(layer_count);
    for (std::uint32_t layer_index = 0; layer_index < layer_count; ++layer_index) {
        NativeRbsongLayerSummary layer;
        layer.version = read_i32(input, "entity layer version");
        if (layer.version < 8 || layer.version > 30) throw std::runtime_error("unsupported rbsong entity layer version");
        if (layer.version >= 23) static_cast<void>(read_i32(input, "entity layer field"));
        layer.file_slot_index = read_i32(input, "entity layer file slot");
        layer.total_object_layers = read_i16(input, "total object layers");
        const auto object_count = read_u32(input, "game object count");
        if (object_count > 100000) throw std::runtime_error("rbsong object count is unreasonably large");
        layer.objects.reserve(object_count);
        for (std::uint32_t object_index = 0; object_index < object_count; ++object_index) {
            layer.objects.push_back(read_object(input, layer.version));
        }
        result.layers.push_back(std::move(layer));
    }
    return result;
}

std::string format_native_rbsong_header_json(const NativeRbsongHeader& header) {
    std::ostringstream output;
    output << "{\n  \"schemaVersion\": 1,\n  \"resourceVersion\": " << header.resource_version
           << ",\n  \"entityVersion\": " << header.entity_version
           << ",\n  \"entityOffset\": " << header.entity_offset
           << ",\n  \"inlineLayerNames\": [";
    for (std::size_t index = 0; index < header.inline_layer_names.size(); ++index) {
        output << (index == 0 ? "" : ", ") << '"' << escape_json(header.inline_layer_names[index]) << '"';
    }
    output << "],\n  \"version15Field\": ";
    if (header.version_15_field) output << *header.version_15_field; else output << "null";
    output << ",\n  \"version16Field\": ";
    if (header.version_16_field) output << *header.version_16_field; else output << "null";
    output << "\n}\n";
    return output.str();
}

std::string format_native_rbsong_tree_json(const NativeRbsongTreeSummary& tree) {
    std::ostringstream output;
    output << "{\n  \"schemaVersion\": 1,\n  \"resourceVersion\": " << tree.header.resource_version
           << ",\n  \"entityVersion\": " << tree.header.entity_version
           << ",\n  \"layers\": [";
    for (std::size_t layer_index = 0; layer_index < tree.layers.size(); ++layer_index) {
        const auto& layer = tree.layers[layer_index];
        output << (layer_index == 0 ? "\n" : ",\n")
               << "    {\"version\": " << layer.version
               << ", \"fileSlotIndex\": " << layer.file_slot_index
               << ", \"totalObjectLayers\": " << layer.total_object_layers
               << ", \"objects\": [";
        for (std::size_t object_index = 0; object_index < layer.objects.size(); ++object_index) {
            const auto& object = layer.objects[object_index];
            output << (object_index == 0 ? "\n" : ",\n")
                   << "      {\"sentinel\": " << (object.sentinel ? "true" : "false")
                   << ", \"layer\": " << object.layer << ", \"index\": " << object.index
                   << ", \"revision\": " << object.revision << ", \"name\": \""
                   << escape_json(object.name) << "\", \"components\": [";
            for (std::size_t component_index = 0; component_index < object.components.size(); ++component_index) {
                const auto& component = object.components[component_index];
                output << (component_index == 0 ? "" : ", ") << "{\"name\": \""
                       << escape_json(component.name) << "\", \"secondaryName\": \""
                       << escape_json(component.secondary_name) << "\", \"revision\": "
                       << component.revision << ", \"unknown\": " << component.unknown
                       << ", \"properties\": [";
                for (std::size_t property_index = 0; property_index < component.properties.size(); ++property_index) {
                    const auto& property = component.properties[property_index];
                    output << (property_index == 0 ? "" : ", ") << "{\"name\": \""
                           << escape_json(property.name) << "\", \"type\": \""
                           << escape_json(property.type) << "\"}";
                }
                output << "]}";
            }
            output << "]}";
        }
        output << "\n    ]}";
    }
    output << "\n  ]\n}\n";
    return output.str();
}

} // namespace rb4pc::inspector