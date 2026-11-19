#include "rb4pc/runtime/mogg_parser.hpp"
#include <iostream>
#include <string>
int main(int argc, char* argv[]) {
    if (argc != 2 && !(argc == 4 && std::string(argv[2]) == "--extract-ogg")) { std::cerr << "Usage: " << argv[0] << " <input.mogg> [--extract-ogg <output.ogg>]\n"; return 2; }
    try { const auto h = rb4pc::runtime::parse_mogg_header(argv[1]);
        std::cout << "MOGG size: " << h.file_size << " bytes\nVersion: " << h.version << "\nOgg offset: " << h.ogg_offset << "\nEmbedded Ogg: " << (h.embedded_ogg ? "yes" : "no") << '\n';
        if (argc == 4) { rb4pc::runtime::extract_mogg_ogg(argv[1], h, argv[3]); std::cout << "Extracted Ogg: " << argv[3] << '\n'; }
        return h.embedded_ogg ? 0 : 1;
    } catch (const std::exception& e) { std::cerr << "rb4pc-mogg-probe: " << e.what() << '\n'; return 1; }
}
