#include "rb4pc/runtime/runtime_session.hpp"
#include "rb4pc/runtime/shell_render.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 2) { std::cerr << "Usage: " << argv[0] << " <header.hdr>\n"; return 2; }
    try {
        const auto session = rb4pc::runtime::RuntimeSession::boot(argv[1]);
        rb4pc::runtime::RuntimeShell shell; shell.source_ready(session.plan().ready);
        rb4pc::runtime::SoftwareFramebuffer framebuffer(1280, 720);
        rb4pc::runtime::render_shell(framebuffer, shell,
            {session.plan().ready, session.vfs().file_count(), session.plan().wave_records});
        rb4pc::runtime::run_window("RB4PC v0.160.0 Pre-alpha Shell", framebuffer);
    } catch (const std::exception& error) { std::cerr << "rb4pc-prealpha: " << error.what() << '\n'; return 1; }
}
