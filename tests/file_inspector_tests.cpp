#include "rb4pc/inspector/file_inspector.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>

int main() {
    const auto path = std::filesystem::temp_directory_path() / "rb4pc-inspector-test.bin";
    const std::array<unsigned char, 4> contents{0x46, 0x4f, 0x52, 0x47};

    {
        std::ofstream output(path, std::ios::binary);
        output.write(reinterpret_cast<const char*>(contents.data()),
                     static_cast<std::streamsize>(contents.size()));
    }

    const auto report = rb4pc::inspector::inspect_file(path);
    std::filesystem::remove(path);

    if (report.size != contents.size() || report.preview_size != contents.size()) {
        std::cerr << "unexpected report size\n";
        return 1;
    }

    for (std::size_t index = 0; index < contents.size(); ++index) {
        if (std::to_integer<unsigned int>(report.preview[index]) != contents[index]) {
            std::cerr << "preview mismatch at byte " << index << '\n';
            return 1;
        }
    }

    return 0;
}

