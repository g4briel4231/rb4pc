#include "rb4pc/runtime/controller_icons.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace rb4pc::runtime {
namespace {

constexpr std::uint32_t background = 0xff20252d;
constexpr std::uint32_t foreground = 0xfff7f7f7;
constexpr std::uint32_t outline = 0xff0b0d10;

void fill_circle(SoftwareFramebuffer& framebuffer, Rect bounds, std::uint32_t color) {
    const auto radius = static_cast<std::int64_t>(std::min(bounds.width, bounds.height) / 2);
    const auto center_x = static_cast<std::int64_t>(bounds.x) + static_cast<std::int64_t>(bounds.width) / 2;
    const auto center_y = static_cast<std::int64_t>(bounds.y) + static_cast<std::int64_t>(bounds.height) / 2;
    for (std::int64_t y = -radius; y <= radius; ++y) {
        for (std::int64_t x = -radius; x <= radius; ++x) {
            if (x * x + y * y <= radius * radius && center_x + x >= 0 && center_y + y >= 0)
                framebuffer.set_pixel(static_cast<std::uint32_t>(center_x + x),
                                      static_cast<std::uint32_t>(center_y + y), color);
        }
    }
}

std::array<std::uint8_t, 7> glyph(char value) {
    switch (value) {
    case 'A': return {0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11};
    case 'B': return {0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e};
    case 'L': return {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f};
    case 'R': return {0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11};
    case 'T': return {0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
    case 'X': return {0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11};
    case 'Y': return {0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04};
    default: throw std::runtime_error("unsupported controller-icon glyph");
    }
}

void draw_label(SoftwareFramebuffer& framebuffer, Rect bounds, std::string_view label,
                std::uint32_t color) {
    if (label.empty()) return;
    const auto available_width = bounds.width > 4 ? bounds.width - 4 : bounds.width;
    const auto available_height = bounds.height > 4 ? bounds.height - 4 : bounds.height;
    const auto units = static_cast<std::uint32_t>(label.size() * 6 - 1);
    const auto scale = std::max<std::uint32_t>(1, std::min(available_width / units, available_height / 7));
    const auto text_width = units * scale;
    const auto text_height = 7 * scale;
    const auto origin_x = bounds.x + (bounds.width > text_width ? (bounds.width - text_width) / 2 : 0);
    const auto origin_y = bounds.y + (bounds.height > text_height ? (bounds.height - text_height) / 2 : 0);
    for (std::size_t character = 0; character < label.size(); ++character) {
        const auto rows = glyph(label[character]);
        for (std::uint32_t row = 0; row < rows.size(); ++row)
            for (std::uint32_t column = 0; column < 5; ++column)
                if ((rows[row] & (1U << (4 - column))) != 0)
                    fill_rect(framebuffer,
                              {origin_x + static_cast<std::uint32_t>(character * 6) * scale + column * scale,
                               origin_y + row * scale, scale, scale}, color);
    }
}

void render_face_button(SoftwareFramebuffer& framebuffer, Rect bounds, char label,
                        std::uint32_t color) {
    fill_circle(framebuffer, bounds, outline);
    Rect inner{bounds.x + 2, bounds.y + 2,
               bounds.width > 4 ? bounds.width - 4 : 0,
               bounds.height > 4 ? bounds.height - 4 : 0};
    fill_circle(framebuffer, inner, color);
    draw_label(framebuffer, inner, std::string_view(&label, 1), foreground);
}

void render_dpad(SoftwareFramebuffer& framebuffer, Rect bounds) {
    const auto arm = std::max<std::uint32_t>(2, std::min(bounds.width, bounds.height) / 3);
    fill_rect(framebuffer, {bounds.x + (bounds.width - arm) / 2, bounds.y, arm, bounds.height}, outline);
    fill_rect(framebuffer, {bounds.x, bounds.y + (bounds.height - arm) / 2, bounds.width, arm}, outline);
    const auto inset = std::max<std::uint32_t>(1, arm / 5);
    fill_rect(framebuffer, {bounds.x + (bounds.width - arm) / 2 + inset, bounds.y + inset,
                            arm > inset * 2 ? arm - inset * 2 : 0,
                            bounds.height > inset * 2 ? bounds.height - inset * 2 : 0}, background);
    fill_rect(framebuffer, {bounds.x + inset, bounds.y + (bounds.height - arm) / 2 + inset,
                            bounds.width > inset * 2 ? bounds.width - inset * 2 : 0,
                            arm > inset * 2 ? arm - inset * 2 : 0}, background);
}

} // namespace

ControllerPrompt controller_prompt_for_action(ControllerAction action, ControllerIconTheme theme) {
    if (theme != ControllerIconTheme::xbox) throw std::runtime_error("unsupported controller icon theme");
    switch (action) {
    case ControllerAction::confirm: return ControllerPrompt::a;
    case ControllerAction::cancel: return ControllerPrompt::b;
    case ControllerAction::navigate: return ControllerPrompt::dpad;
    case ControllerAction::previous_tab: return ControllerPrompt::left_bumper;
    case ControllerAction::next_tab: return ControllerPrompt::right_bumper;
    case ControllerAction::pause: return ControllerPrompt::menu;
    case ControllerAction::secondary_view: return ControllerPrompt::view;
    }
    throw std::runtime_error("unsupported controller action");
}

void render_controller_icon(SoftwareFramebuffer& framebuffer, ControllerPrompt prompt,
                            Rect bounds, ControllerIconTheme theme) {
    if (theme != ControllerIconTheme::xbox) throw std::runtime_error("unsupported controller icon theme");
    if (bounds.width == 0 || bounds.height == 0) return;
    switch (prompt) {
    case ControllerPrompt::a: render_face_button(framebuffer, bounds, 'A', 0xff3aa655); return;
    case ControllerPrompt::b: render_face_button(framebuffer, bounds, 'B', 0xffd64a45); return;
    case ControllerPrompt::x: render_face_button(framebuffer, bounds, 'X', 0xff318ac7); return;
    case ControllerPrompt::y: render_face_button(framebuffer, bounds, 'Y', 0xffe2b93b); return;
    case ControllerPrompt::dpad: render_dpad(framebuffer, bounds); return;
    case ControllerPrompt::menu:
        fill_circle(framebuffer, bounds, background);
        for (std::uint32_t index = 0; index < 3; ++index)
            fill_rect(framebuffer, {bounds.x + bounds.width / 4, bounds.y + bounds.height / 3 + index * 4,
                                    bounds.width / 2, 2}, foreground);
        return;
    case ControllerPrompt::view:
        fill_circle(framebuffer, bounds, background);
        stroke_rect(framebuffer, {bounds.x + bounds.width / 4, bounds.y + bounds.height / 3,
                                  bounds.width / 2, bounds.height / 3}, foreground, 2);
        return;
    default: break;
    }

    fill_rect(framebuffer, bounds, outline);
    const Rect inner{bounds.x + 2, bounds.y + 2,
                     bounds.width > 4 ? bounds.width - 4 : 0,
                     bounds.height > 4 ? bounds.height - 4 : 0};
    fill_rect(framebuffer, inner, background);
    switch (prompt) {
    case ControllerPrompt::left_bumper: draw_label(framebuffer, inner, "LB", foreground); break;
    case ControllerPrompt::right_bumper: draw_label(framebuffer, inner, "RB", foreground); break;
    case ControllerPrompt::left_trigger: draw_label(framebuffer, inner, "LT", foreground); break;
    case ControllerPrompt::right_trigger: draw_label(framebuffer, inner, "RT", foreground); break;
    case ControllerPrompt::left_stick: draw_label(framebuffer, inner, "L", foreground); break;
    case ControllerPrompt::right_stick: draw_label(framebuffer, inner, "R", foreground); break;
    default: throw std::runtime_error("unsupported Xbox controller prompt");
    }
}

} // namespace rb4pc::runtime
