#include "rb4pc/runtime/rbmid_parser.hpp"

#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 2) { std::cerr << "Usage: " << argv[0] << " <input.rbmid_pc>\n"; return 2; }
    try {
        const auto header = rb4pc::runtime::parse_rbmid_header(argv[1]);
        const auto format = header.format == rb4pc::runtime::RBMidFormat::rb4 ? "RB4" : "RBVR";
        std::cout << "RBMID size: " << header.file_size << " bytes\nFormat: " << format << '\n';
    } catch (const std::exception& error) { std::cerr << "rb4pc-rbmid-probe: " << error.what() << '\n'; return 1; }
}
