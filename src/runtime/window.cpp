#include "rb4pc/runtime/window.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace rb4pc::runtime {
SoftwareFramebuffer::SoftwareFramebuffer(std::uint32_t width, std::uint32_t height)
    : width_(width), height_(height) {
    if (width == 0 || height == 0 || width > std::numeric_limits<std::size_t>::max() / height)
        throw std::runtime_error("invalid framebuffer dimensions");
    pixels_.resize(static_cast<std::size_t>(width) * height);
}
std::uint32_t SoftwareFramebuffer::width() const { return width_; }
std::uint32_t SoftwareFramebuffer::height() const { return height_; }
void SoftwareFramebuffer::clear(std::uint32_t color) { std::fill(pixels_.begin(), pixels_.end(), color); }
void SoftwareFramebuffer::set_pixel(std::uint32_t x, std::uint32_t y, std::uint32_t color) {
    if (x >= width_ || y >= height_) throw std::runtime_error("pixel is outside framebuffer");
    pixels_[static_cast<std::size_t>(y) * width_ + x] = color;
}
const std::vector<std::uint32_t>& SoftwareFramebuffer::pixels() const { return pixels_; }
bool window_backend_available() {
#ifdef _WIN32
    return true;
#else
    return false;
#endif
}

#ifdef _WIN32
namespace {
LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(window, message, wparam, lparam);
}
}
#endif

void run_window(std::string_view title, SoftwareFramebuffer& framebuffer) {
#ifdef _WIN32
    const auto instance = GetModuleHandleW(nullptr);
    const wchar_t* class_name = L"RB4PCWindow";
    WNDCLASSW window_class{}; window_class.lpfnWndProc = window_proc; window_class.hInstance = instance;
    window_class.lpszClassName = class_name; window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&window_class);
    const std::wstring wide_title(title.begin(), title.end());
    HWND window = CreateWindowExW(0, class_name, wide_title.c_str(), WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, static_cast<int>(framebuffer.width()), static_cast<int>(framebuffer.height()),
        nullptr, nullptr, instance, nullptr);
    if (!window) throw std::runtime_error("CreateWindowExW failed");
    BITMAPINFO bitmap{}; bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmap.bmiHeader.biWidth = static_cast<LONG>(framebuffer.width());
    bitmap.bmiHeader.biHeight = -static_cast<LONG>(framebuffer.height()); bitmap.bmiHeader.biPlanes = 1;
    bitmap.bmiHeader.biBitCount = 32; bitmap.bmiHeader.biCompression = BI_RGB;
    MSG message{}; bool running = true;
    while (running) {
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) running = false;
            TranslateMessage(&message); DispatchMessageW(&message);
        }
        if (!running) break;
        RECT client{}; GetClientRect(window, &client); HDC dc = GetDC(window);
        StretchDIBits(dc, 0, 0, client.right, client.bottom, 0, 0, framebuffer.width(), framebuffer.height(),
            framebuffer.pixels().data(), &bitmap, DIB_RGB_COLORS, SRCCOPY);
        ReleaseDC(window, dc); Sleep(16);
    }
#else
    (void)title; (void)framebuffer; throw std::runtime_error("window backend is unavailable");
#endif
}
} // namespace rb4pc::runtime
