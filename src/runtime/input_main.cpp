#include "rb4pc/runtime/input.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
bool differs(const rb4pc::runtime::ControllerState& left,
             const rb4pc::runtime::ControllerState& right) {
    return left.connected != right.connected || left.buttons != right.buttons ||
           left.left_trigger != right.left_trigger || left.right_trigger != right.right_trigger ||
           left.left_x != right.left_x || left.left_y != right.left_y ||
           left.right_x != right.right_x || left.right_y != right.right_y;
}

void print_state(const rb4pc::runtime::ControllerState& state) {
    std::cout << "Controller " << state.index << ": "
              << (state.connected ? "connected" : "not connected");
    if (state.connected)
        std::cout << ", buttons=0x" << std::hex << state.buttons << std::dec
                  << ", LT=" << static_cast<unsigned int>(state.left_trigger)
                  << ", RT=" << static_cast<unsigned int>(state.right_trigger)
                  << ", LX=" << state.left_x << ", LY=" << state.left_y
                  << ", RX=" << state.right_x << ", RY=" << state.right_y;
    std::cout << '\n';
}
}

int main(int argc, char* argv[]) {
    if (!rb4pc::runtime::input_backend_available()) {
        std::cerr << "No input backend available\n";
        return 1;
    }
    int watch_seconds = 0;
    try {
        if (argc == 3 && std::string(argv[1]) == "--watch") {
            watch_seconds = std::stoi(argv[2]);
            if (watch_seconds < 1 || watch_seconds > 120)
                throw std::runtime_error("watch duration must be between 1 and 120 seconds");
        } else if (argc != 1) {
            std::cerr << "Usage: " << argv[0] << " [--watch <seconds>]\n";
            return 2;
        }
    } catch (const std::exception& error) {
        std::cerr << "rb4pc-input-probe: " << error.what() << '\n';
        return 2;
    }

    auto states = rb4pc::runtime::poll_controllers();
    for (const auto& state : states) print_state(state);
    if (watch_seconds == 0) return 0;

    std::array<bool, 4> observed{};
    std::array<std::uint16_t, 4> buttons_seen{};
    std::array<std::uint8_t, 4> max_left_trigger{}, max_right_trigger{};
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(watch_seconds);
    while (std::chrono::steady_clock::now() < deadline) {
        const auto current = rb4pc::runtime::poll_controllers();
        for (std::size_t index = 0; index < current.size() && index < observed.size(); ++index) {
            if (current[index].connected) {
                observed[index] = true;
                buttons_seen[index] = static_cast<std::uint16_t>(buttons_seen[index] | current[index].buttons);
                max_left_trigger[index] = std::max(max_left_trigger[index], current[index].left_trigger);
                max_right_trigger[index] = std::max(max_right_trigger[index], current[index].right_trigger);
            }
            if (index >= states.size() || differs(states[index], current[index])) print_state(current[index]);
        }
        states = current;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    std::cout << "Observation summary:\n";
    for (std::size_t index = 0; index < observed.size(); ++index)
        std::cout << "  Controller " << index << ": observed=" << (observed[index] ? "yes" : "no")
                  << ", buttonsSeen=0x" << std::hex << buttons_seen[index] << std::dec
                  << ", maxLT=" << static_cast<unsigned int>(max_left_trigger[index])
                  << ", maxRT=" << static_cast<unsigned int>(max_right_trigger[index]) << '\n';
    return std::any_of(observed.begin(), observed.end(), [](bool value) { return value; }) ? 0 : 1;
}
