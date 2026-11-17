#include "rb4pc/inspector/file_inspector.hpp"
#include "rb4pc/inspector/payload_catalog.hpp"
#include "rb4pc/inspector/record_extractor.hpp"

#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

namespace {

void print_usage(const char* program) {
    std::cerr << "Usage: " << program
              << " <header.hdr> [--find <internal-path>] [--catalog]"
                 " [--extract <internal-path> --destination <file>] [--output <report.json>]\n";
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 2;
    }

    std::filesystem::path input;
    std::optional<std::filesystem::path> output_path;
    std::optional<std::string> find_path;
    bool catalog = false;
    std::optional<std::string> extract_path;
    std::optional<std::filesystem::path> destination_path;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--output") {
            if (++index >= argc || output_path) {
                print_usage(argv[0]);
                return 2;
            }
            output_path = argv[index];
        } else if (argument == "--find") {
            if (++index >= argc || find_path) {
                print_usage(argv[0]);
                return 2;
            }
            find_path = argv[index];
        } else if (argument == "--catalog") {
            if (catalog) {
                print_usage(argv[0]);
                return 2;
            }
            catalog = true;
        } else if (argument == "--extract") {
            if (++index >= argc || extract_path) {
                print_usage(argv[0]);
                return 2;
            }
            extract_path = argv[index];
        } else if (argument == "--destination") {
            if (++index >= argc || destination_path) {
                print_usage(argv[0]);
                return 2;
            }
            destination_path = argv[index];
        } else if (input.empty()) {
            input = argument;
        } else {
            print_usage(argv[0]);
            return 2;
        }
    }

    if (input.empty()) {
        print_usage(argv[0]);
        return 2;
    }
    if (extract_path.has_value() != destination_path.has_value()) {
        print_usage(argv[0]);
        return 2;
    }

    try {
        const auto report = rb4pc::inspector::inspect_header_set(input);
        std::cout << rb4pc::inspector::format_report(report);
        if (find_path) {
            const auto preview = rb4pc::inspector::inspect_record(report, *find_path);
            if (!preview) throw std::runtime_error("internal path was not found: " + *find_path);
            std::cout << rb4pc::inspector::format_report(report, *preview);
        }
        if (catalog) std::cout << rb4pc::inspector::format_report(rb4pc::inspector::catalog_payloads(report));
        if (extract_path) {
            const auto result = rb4pc::inspector::extract_record(report, *extract_path, *destination_path);
            std::cout << "Extracted record: " << result.destination.string() << '\n'
                      << "Extracted size: " << result.size << " bytes\n"
                      << "Extracted SHA-256: " << result.sha256 << '\n';
        }
        if (output_path) {
            std::ofstream output(*output_path, std::ios::binary);
            if (!output) throw std::runtime_error("could not create report: " + output_path->string());
            output << rb4pc::inspector::format_json(report);
            if (!output) throw std::runtime_error("could not write report: " + output_path->string());
            std::cout << "JSON report: " << output_path->string() << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << "forge-inspector: " << error.what() << '\n';
        return 1;
    }
    return 0;
}

