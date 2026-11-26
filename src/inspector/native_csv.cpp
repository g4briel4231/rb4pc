#include "rb4pc/inspector/native_csv.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace rb4pc::inspector {
namespace {

std::uint32_t take_u32(const std::vector<unsigned char>& bytes, std::size_t& position) {
    if (position > bytes.size() || bytes.size() - position < 4) {
        throw std::runtime_error("native CSV ended while reading a 32-bit field");
    }
    const auto value = static_cast<std::uint32_t>(bytes[position]) |
                       static_cast<std::uint32_t>(bytes[position + 1]) << 8 |
                       static_cast<std::uint32_t>(bytes[position + 2]) << 16 |
                       static_cast<std::uint32_t>(bytes[position + 3]) << 24;
    position += 4;
    return value;
}

std::string string_at(const std::vector<unsigned char>& table, std::uint32_t offset) {
    if (offset >= table.size()) throw std::runtime_error("native CSV string offset is outside the table");
    const auto begin = table.begin() + offset;
    const auto end = std::find(begin, table.end(), 0);
    if (end == table.end()) throw std::runtime_error("native CSV string is not NUL terminated");
    return {begin, end};
}

std::string escape_json(std::string_view value) {
    std::ostringstream output;
    for (const unsigned char character : value) {
        if (character == '"' || character == '\\') output << '\\' << static_cast<char>(character);
        else if (character == '\n') output << "\\n";
        else if (character == '\r') output << "\\r";
        else if (character == '\t') output << "\\t";
        else if (character < 0x20) {
            output << "\\u00" << "0123456789abcdef"[character >> 4]
                   << "0123456789abcdef"[character & 0x0f];
        } else output << static_cast<char>(character);
    }
    return output.str();
}

} // namespace

NativeCsv read_native_csv(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("could not open native CSV: " + path.string());
    const std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(input),
                                           std::istreambuf_iterator<char>()};
    std::size_t position = 0;
    NativeCsv result;
    result.version = take_u32(bytes, position);
    result.encoding = take_u32(bytes, position);
    if (result.version != 1 || result.encoding != 2) {
        throw std::runtime_error("unsupported native CSV version or encoding");
    }
    if (position >= bytes.size()) throw std::runtime_error("native CSV separator is missing");
    result.separator = bytes[position++];
    if (result.separator != ',') throw std::runtime_error("unsupported native CSV separator");

    const auto table_size = static_cast<std::size_t>(take_u32(bytes, position));
    if (table_size > bytes.size() - position) throw std::runtime_error("native CSV string table exceeds the file");
    const std::vector<unsigned char> table(bytes.begin() + position, bytes.begin() + position + table_size);
    position += table_size;

    const auto header_count = static_cast<std::size_t>(take_u32(bytes, position));
    if (header_count > (bytes.size() - position) / 4) throw std::runtime_error("native CSV header count exceeds the file");
    result.headers.reserve(header_count);
    for (std::size_t index = 0; index < header_count; ++index) {
        result.headers.push_back(string_at(table, take_u32(bytes, position)));
    }

    const auto row_count = static_cast<std::size_t>(take_u32(bytes, position));
    if (row_count > (bytes.size() - position) / 4) throw std::runtime_error("native CSV row count exceeds the file");
    result.rows.reserve(row_count);
    for (std::size_t row_index = 0; row_index < row_count; ++row_index) {
        const auto cell_count = static_cast<std::size_t>(take_u32(bytes, position));
        if (cell_count > (bytes.size() - position) / 4) throw std::runtime_error("native CSV cell count exceeds the file");
        auto& row = result.rows.emplace_back();
        row.reserve(cell_count);
        for (std::size_t cell_index = 0; cell_index < cell_count; ++cell_index) {
            row.push_back(string_at(table, take_u32(bytes, position)));
        }
    }
    if (position != bytes.size()) throw std::runtime_error("native CSV has unconsumed trailing bytes");
    return result;
}

std::string format_native_csv_summary_json(const NativeCsv& csv) {
    std::size_t minimum_columns = csv.rows.empty() ? 0 : std::numeric_limits<std::size_t>::max();
    std::size_t maximum_columns = 0;
    for (const auto& row : csv.rows) {
        minimum_columns = std::min(minimum_columns, row.size());
        maximum_columns = std::max(maximum_columns, row.size());
    }
    std::ostringstream output;
    output << "{\n  \"schemaVersion\": 1,\n  \"formatVersion\": " << csv.version
           << ",\n  \"encoding\": " << csv.encoding
           << ",\n  \"separator\": \"" << static_cast<char>(csv.separator)
           << "\",\n  \"headerCount\": " << csv.headers.size()
           << ",\n  \"rowCount\": " << csv.rows.size()
           << ",\n  \"minimumRowColumns\": " << minimum_columns
           << ",\n  \"maximumRowColumns\": " << maximum_columns
           << ",\n  \"headers\": [";
    for (std::size_t index = 0; index < csv.headers.size(); ++index) {
        output << (index == 0 ? "" : ", ") << '"' << escape_json(csv.headers[index]) << '"';
    }
    output << "]\n}\n";
    return output.str();
}

} // namespace rb4pc::inspector