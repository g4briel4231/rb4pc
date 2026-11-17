#include "rb4pc/inspector/file_inspector.hpp"
#include "rb4pc/inspector/payload_catalog.hpp"

#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

namespace {

void print_usage(const char* program) {
    std::cerr << "Usage: " << program
              << " <header.hdr> [--find <internal-path>] [--catalog] [--output <report.json>]\n";
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

    try {
        const auto report = rb4pc::inspector::inspect_header_set(input);
        std::cout << rb4pc::inspector::format_report(report);
        if (find_path) {
            const auto preview = rb4pc::inspector::inspect_record(report, *find_path);
            if (!preview) throw std::runtime_error("internal path was not found: " + *find_path);
            std::cout << rb4pc::inspector::format_report(report, *preview);
        }
        if (catalog) std::cout << rb4pc::inspector::format_report(rb4pc::inspector::catalog_payloads(report));
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

