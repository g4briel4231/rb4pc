#pragma once
#include "rb4pc/runtime/shell.hpp"

namespace rb4pc::runtime {
struct ShellRenderStatus { bool source_ready = false; std::size_t mounted_records = 0; std::size_t wave_records = 0; };
void render_shell(SoftwareFramebuffer& framebuffer, const RuntimeShell& shell, const ShellRenderStatus& status);
} // namespace rb4pc::runtime
