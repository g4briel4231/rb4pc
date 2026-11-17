#include "rb4pc/runtime/input.hpp"
#include <iostream>

int main() {
    if (!rb4pc::runtime::input_backend_available()) { std::cerr << "No input backend available\n"; return 1; }
    const auto states = rb4pc::runtime::poll_controllers();
    for (const auto& state : states) {
        std::cout << "Controller " << state.index << ": " << (state.connected ? "connected" : "not connected");
        if (state.connected) std::cout << ", buttons=0x" << std::hex << state.buttons;
        std::cout << '\n';
    }
}
