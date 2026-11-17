cmake_minimum_required(VERSION 3.25)

project(RB4PC VERSION 0.26.0 LANGUAGES CXX)

option(RB4PC_BUILD_TESTS "Build RB4PC tests" ON)

add_library(rb4pc_inspection
    src/inspector/asset_converter.cpp
    src/inspector/archive_layout.cpp
    src/inspector/binary_analysis.cpp
    src/builder/build_plan.cpp
    src/builder/song_layout.cpp
    src/builder/custom_song_catalog.cpp
    src/builder/install_plan.cpp
    src/inspector/file_inspector.cpp
    src/inspector/hdr_decoder.cpp
    src/inspector/murmur_hash3.cpp
    src/inspector/music_catalog.cpp
    src/inspector/payload_catalog.cpp
    src/inspector/record_extractor.cpp
    src/inspector/riff_parser.cpp
    src/inspector/sha256.cpp
    src/runtime/archive_file_system.cpp
    src/runtime/wave_loader.cpp
    src/runtime/pcm_stream.cpp
    src/runtime/runtime_session.cpp
    src/runtime/audio_output.cpp
    src/runtime/input.cpp
    src/runtime/window.cpp
    src/runtime/ui.cpp
    src/runtime/gameplay_timing.cpp
    src/runtime/shell.cpp
    src/runtime/shell_render.cpp
    src/runtime/midi_parser.cpp
    src/runtime/five_fret_chart.cpp
)
target_include_directories(rb4pc_inspection PUBLIC include)
target_compile_features(rb4pc_inspection PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(rb4pc_inspection PRIVATE /W4 /permissive-)
else()
    target_compile_options(rb4pc_inspection PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(rb4pc-forge-inspector
    src/inspector/main.cpp
)
target_link_libraries(rb4pc-forge-inspector PRIVATE rb4pc_inspection)

add_executable(rb4pc-builder src/builder/main.cpp)
target_link_libraries(rb4pc-builder PRIVATE rb4pc_inspection)

add_executable(rb4pc-runtime-probe src/runtime/main.cpp)
target_link_libraries(rb4pc-runtime-probe PRIVATE rb4pc_inspection)

add_executable(rb4pc-audio-probe src/runtime/audio_main.cpp)
target_link_libraries(rb4pc-audio-probe PRIVATE rb4pc_inspection)

add_executable(rb4pc-audio-play src/runtime/audio_play_main.cpp)
target_link_libraries(rb4pc-audio-play PRIVATE rb4pc_inspection)

if(WIN32)
    target_link_libraries(rb4pc_inspection PRIVATE winmm xinput user32 gdi32)
endif()

add_executable(rb4pc-input-probe src/runtime/input_main.cpp)
target_link_libraries(rb4pc-input-probe PRIVATE rb4pc_inspection)

add_executable(rb4pc-window src/runtime/window_main.cpp)
target_link_libraries(rb4pc-window PRIVATE rb4pc_inspection)

add_executable(rb4pc-prealpha src/runtime/prealpha_main.cpp)
target_link_libraries(rb4pc-prealpha PRIVATE rb4pc_inspection)

add_executable(rb4pc-midi-probe src/runtime/midi_main.cpp)
target_link_libraries(rb4pc-midi-probe PRIVATE rb4pc_inspection)

add_executable(rb4pc-runtime src/runtime/session_main.cpp)
target_link_libraries(rb4pc-runtime PRIVATE rb4pc_inspection)

if(RB4PC_BUILD_TESTS)
    enable_testing()
    add_executable(rb4pc-inspector-tests tests/file_inspector_tests.cpp)
    target_link_libraries(rb4pc-inspector-tests PRIVATE rb4pc_inspection)
    add_test(NAME rb4pc-inspector-tests COMMAND rb4pc-inspector-tests)
endif()

