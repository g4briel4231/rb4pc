cmake_minimum_required(VERSION 3.25)

project(RB4PC VERSION 0.620.0 LANGUAGES CXX)

option(RB4PC_BUILD_TESTS "Build RB4PC tests" ON)

include(FetchContent)
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 6b5799bae8a1fe3792691ef4b72ae26434c3e799
)
FetchContent_MakeAvailable(stb)

add_library(rb4pc_inspection
    src/inspector/asset_converter.cpp
    src/inspector/archive_layout.cpp
    src/inspector/binary_analysis.cpp
    src/builder/build_plan.cpp
    src/builder/song_layout.cpp
    src/builder/custom_song_catalog.cpp
    src/builder/install_plan.cpp
    src/builder/midi_session_plan.cpp
    src/builder/song_session_plan.cpp
    src/builder/song_assembler.cpp
    src/builder/pc_archive_builder.cpp
    src/builder/pc_asset_catalog.cpp
    src/builder/pc_audio_catalog.cpp
    src/builder/pc_character_catalog.cpp
    src/builder/pc_corpus_audit.cpp
    src/builder/pc_metadata_catalog.cpp
    src/inspector/file_inspector.cpp
    src/inspector/hdr_decoder.cpp
    src/inspector/murmur_hash3.cpp
    src/inspector/music_catalog.cpp
    src/inspector/payload_catalog.cpp
    src/inspector/record_extractor.cpp
    src/inspector/riff_parser.cpp
    src/inspector/sha256.cpp
    src/inspector/texture_container.cpp
    src/runtime/archive_file_system.cpp
    src/runtime/wave_loader.cpp
    src/runtime/pcm_stream.cpp
    src/runtime/runtime_session.cpp
    src/runtime/audio_output.cpp
    src/runtime/input.cpp
    src/runtime/app_config.cpp
    src/runtime/window.cpp
    src/runtime/ui.cpp
    src/runtime/text.cpp
    src/runtime/controller_icons.cpp
    src/runtime/gameplay_timing.cpp
    src/runtime/shell.cpp
    src/runtime/shell_render.cpp
    src/runtime/midi_parser.cpp
    src/runtime/rbmid_parser.cpp
    src/runtime/five_fret_import.cpp
    src/runtime/mogg_parser.cpp
    src/runtime/vorbis_decoder.cpp
    src/runtime/vorbis_playback_queue.cpp
    src/runtime/mogg_mix.cpp
    src/runtime/mogg_audio_output.cpp
    src/runtime/song_session_verifier.cpp
    src/runtime/song_directory.cpp
    src/runtime/song_library.cpp
    src/runtime/song_browser.cpp
    src/runtime/artwork_texture.cpp
    src/runtime/interactive_song.cpp
    src/runtime/five_fret_chart.cpp
    src/runtime/timed_chart.cpp
    src/runtime/five_fret_gameplay.cpp
    src/runtime/note_highway.cpp
    src/runtime/five_fret_input.cpp
    src/runtime/five_fret_session.cpp
    src/runtime/audio_transport.cpp
    src/runtime/pcm_playback_queue.cpp
)
target_include_directories(rb4pc_inspection PUBLIC include)
target_include_directories(rb4pc_inspection PRIVATE ${stb_SOURCE_DIR})
target_compile_features(rb4pc_inspection PUBLIC cxx_std_20)

if(MSVC)
    set_source_files_properties(src/runtime/vorbis_decoder.cpp PROPERTIES COMPILE_OPTIONS /w)
else()
    set_source_files_properties(src/runtime/vorbis_decoder.cpp PROPERTIES COMPILE_OPTIONS -w)
endif()

if(MSVC)
    target_compile_options(rb4pc_inspection PRIVATE /W4 /permissive-)
else()
    target_compile_options(rb4pc_inspection PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(rb4pc-forge-inspector
    src/inspector/main.cpp
)
target_link_libraries(rb4pc-forge-inspector PRIVATE rb4pc_inspection)

add_executable(rb4pc-texture-probe src/inspector/texture_main.cpp)
target_link_libraries(rb4pc-texture-probe PRIVATE rb4pc_inspection)

add_executable(rb4pc-builder src/builder/main.cpp)
target_link_libraries(rb4pc-builder PRIVATE rb4pc_inspection)

if(WIN32)
    add_executable(rb4pc-builder-gui WIN32 src/builder/gui_main.cpp)
    target_compile_definitions(rb4pc-builder-gui PRIVATE
        RB4PC_VERSION_W=L"${PROJECT_VERSION}")
    target_link_libraries(rb4pc-builder-gui PRIVATE rb4pc_inspection gdiplus ole32 shell32 comdlg32)
endif()

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

add_executable(rb4pc-rbmid-probe src/runtime/rbmid_main.cpp)
target_link_libraries(rb4pc-rbmid-probe PRIVATE rb4pc_inspection)

add_executable(rb4pc-session-probe src/runtime/session_probe_main.cpp)
target_link_libraries(rb4pc-session-probe PRIVATE rb4pc_inspection)

add_executable(rb4pc-mogg-probe src/runtime/mogg_main.cpp)
target_link_libraries(rb4pc-mogg-probe PRIVATE rb4pc_inspection)

add_executable(rb4pc-mogg-play src/runtime/mogg_play_main.cpp)
target_link_libraries(rb4pc-mogg-play PRIVATE rb4pc_inspection)

add_executable(rb4pc-song-verify src/runtime/song_verify_main.cpp)
target_link_libraries(rb4pc-song-verify PRIVATE rb4pc_inspection)

add_executable(rb4pc-song-play src/runtime/song_play_main.cpp)
target_link_libraries(rb4pc-song-play PRIVATE rb4pc_inspection)

add_executable(rb4pc-song-library src/runtime/song_library_main.cpp)
target_link_libraries(rb4pc-song-library PRIVATE rb4pc_inspection)

add_executable(rb4pc-launcher src/runtime/launcher_main.cpp)
target_link_libraries(rb4pc-launcher PRIVATE rb4pc_inspection)

add_executable(rb4pc-runtime src/runtime/session_main.cpp)
target_link_libraries(rb4pc-runtime PRIVATE rb4pc_inspection)

if(WIN32)
    foreach(target IN ITEMS
        rb4pc-forge-inspector rb4pc-texture-probe rb4pc-builder
        rb4pc-runtime-probe rb4pc-audio-probe rb4pc-audio-play
        rb4pc-input-probe rb4pc-window rb4pc-prealpha rb4pc-midi-probe
        rb4pc-rbmid-probe rb4pc-session-probe rb4pc-mogg-probe
        rb4pc-mogg-play rb4pc-song-verify rb4pc-song-play
        rb4pc-song-library rb4pc-launcher rb4pc-runtime)
        target_sources(${target} PRIVATE assets/windows/rb4pc.rc)
    endforeach()
    target_sources(rb4pc-builder-gui PRIVATE assets/windows/rb4pc.rc)
endif()

if(RB4PC_BUILD_TESTS)
    enable_testing()
    add_executable(rb4pc-inspector-tests tests/file_inspector_tests.cpp)
    target_link_libraries(rb4pc-inspector-tests PRIVATE rb4pc_inspection)
    add_test(NAME rb4pc-inspector-tests COMMAND rb4pc-inspector-tests)
endif()

install(TARGETS
    rb4pc-launcher rb4pc-builder rb4pc-song-verify rb4pc-song-play
    rb4pc-song-library rb4pc-texture-probe
    RUNTIME DESTINATION .)
if(WIN32)
    install(TARGETS rb4pc-builder-gui RUNTIME DESTINATION .)
endif()
install(FILES README.md THIRD_PARTY_NOTICES.md DESTINATION .)
install(FILES config/rb4pc.ini DESTINATION .)
install(DIRECTORY docs/ DESTINATION docs FILES_MATCHING PATTERN "*.md")
install(DIRECTORY assets/branding/ DESTINATION assets/branding)
install(DIRECTORY DESTINATION songs)

if(WIN32)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|aarch64)$")
        set(RB4PC_PACKAGE_ARCH "arm64")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(RB4PC_PACKAGE_ARCH "x64")
    else()
        set(RB4PC_PACKAGE_ARCH "x86")
    endif()
    set(RB4PC_PACKAGE_PLATFORM "win")
else()
    string(TOLOWER "${CMAKE_SYSTEM_NAME}" RB4PC_PACKAGE_PLATFORM)
    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" RB4PC_PACKAGE_ARCH)
endif()
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME "RB4PC")
set(CPACK_PACKAGE_VENDOR "RB4PC contributors")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "rb4pc-${PROJECT_VERSION}-${RB4PC_PACKAGE_PLATFORM}-${RB4PC_PACKAGE_ARCH}")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
include(CPack)

