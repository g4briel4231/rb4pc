cmake_minimum_required(VERSION 3.25)

project(RB4PC VERSION 0.6.1 LANGUAGES CXX)

option(RB4PC_BUILD_TESTS "Build RB4PC tests" ON)

add_library(rb4pc_inspection
    src/inspector/asset_converter.cpp
    src/inspector/archive_layout.cpp
    src/inspector/binary_analysis.cpp
    src/builder/build_plan.cpp
    src/inspector/file_inspector.cpp
    src/inspector/hdr_decoder.cpp
    src/inspector/murmur_hash3.cpp
    src/inspector/payload_catalog.cpp
    src/inspector/record_extractor.cpp
    src/inspector/riff_parser.cpp
    src/inspector/sha256.cpp
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

if(RB4PC_BUILD_TESTS)
    enable_testing()
    add_executable(rb4pc-inspector-tests tests/file_inspector_tests.cpp)
    target_link_libraries(rb4pc-inspector-tests PRIVATE rb4pc_inspection)
    add_test(NAME rb4pc-inspector-tests COMMAND rb4pc-inspector-tests)
endif()

