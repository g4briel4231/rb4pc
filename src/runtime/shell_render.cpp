#include "rb4pc/runtime/shell_render.hpp"

namespace rb4pc::runtime {
void render_shell(SoftwareFramebuffer& fb, const RuntimeShell& shell, const ShellRenderStatus& status) {
    fb.clear(0xff10131a);
    fill_rect(fb, {0, 0, fb.width(), 64}, status.source_ready ? 0xff184f36 : 0xff5f2020);
    const auto panel_width = fb.width() > 160 ? fb.width() - 160 : fb.width();
    fill_rect(fb, {80, 96, panel_width, fb.height() > 160 ? fb.height() - 160 : 0}, 0xff202938);
    stroke_rect(fb, {80, 96, panel_width, fb.height() > 160 ? fb.height() - 160 : 0}, 0xff4d8ac7, 3);
    if (shell.screen() == ShellScreen::main_menu) {
        for (std::size_t i = 0; i < shell.main_menu().items().size(); ++i) {
            const auto color = i == shell.main_menu().selected_index() ? 0xffd08a28 : 0xff39485e;
            fill_rect(fb, {120, static_cast<std::uint32_t>(130 + i * 58), panel_width > 80 ? panel_width - 80 : 0, 40}, color);
        }
    }
    const auto record_bar = static_cast<std::uint32_t>(status.mounted_records % (fb.width() ? fb.width() : 1));
    const auto wave_bar = static_cast<std::uint32_t>(status.wave_records % (fb.width() ? fb.width() : 1));
    fill_rect(fb, {0, fb.height() > 24 ? fb.height() - 24 : 0, record_bar, 8}, 0xff54a6ff);
    fill_rect(fb, {0, fb.height() > 12 ? fb.height() - 12 : 0, wave_bar, 8}, 0xff64d27b);
}
} // namespace rb4pc::runtime
