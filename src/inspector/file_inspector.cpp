#include "rb4pc/inspector/file_inspector.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace rb4pc::inspector {

FileReport inspect_file(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("file does not exist: " + path.string());
    }
    if (!std::filesystem::is_regular_file(path)) {
        throw std::runtime_error("path is not a regular file: " + path.string());
    }

    FileReport report;
    report.path = path;
    report.size = std::filesystem::file_size(path);

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("could not open file: " + path.string());
    }

    input.read(reinterpret_cast<char*>(report.preview.data()),
               static_cast<std::streamsize>(report.preview.size()));
    report.preview_size = static_cast<std::size_t>(input.gcount());
    return report;
}

std::string format_report(const FileReport& report) {
    std::ostringstream output;
    output << "Path: " << report.path.string() << '\n';
    output << "Size: " << report.size << " bytes\n";
    output << "First " << report.preview_size << " bytes:";

    if (report.preview_size == 0) {
        output << " <empty>\n";
        return output.str();
    }

    output << std::hex << std::setfill('0');
    for (std::size_t index = 0; index < report.preview_size; ++index) {
        if (index % 16 == 0) {
            output << '\n';
        } else {
            output << ' ';
        }
        output << std::setw(2) << std::to_integer<unsigned int>(report.preview[index]);
    }
    output << '\n';
    return output.str();
}

} // namespace rb4pc::inspector

