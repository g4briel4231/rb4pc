#include "rb4pc/runtime/archive_file_system.hpp"

#include <iomanip>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <header.hdr> <internal-path>\n";
        return 2;
    }
    try {
        const auto vfs = rb4pc::runtime::ArchiveFileSystem::mount(argv[1]);
        const auto size = vfs.file_size(argv[2]);
        if (!size) throw std::runtime_error("internal path was not found");
        const auto preview_size = static_cast<std::size_t>(std::min<std::uint64_t>(*size, 32));
        const auto bytes = vfs.read(argv[2], 0, preview_size);
        std::cout << "Mounted records: " << vfs.file_count() << '\n'
                  << "File size: " << *size << " bytes\nPreview:";
        for (const auto value : bytes) std::cout << ' ' << std::hex << std::setw(2) << std::setfill('0') << std::to_integer<unsigned int>(value);
        std::cout << '\n';
    } catch (const std::exception& error) {
        std::cerr << "rb4pc-runtime-probe: " << error.what() << '\n';
        return 1;
    }
}
