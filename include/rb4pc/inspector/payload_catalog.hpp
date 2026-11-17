#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace rb4pc::inspector {

struct HeaderSetReport;

struct PayloadCatalog {
    std::size_t total_records = 0;
    std::size_t analyzed_records = 0;
    std::size_t empty_records = 0;
    std::size_t unreadable_records = 0;
    std::map<std::string, std::size_t> extensions;
    std::map<std::string, std::size_t> signatures;
    std::map<std::string, std::vector<std::string>> signature_examples;
    std::size_t riff_valid_records = 0;
    std::size_t riff_invalid_records = 0;
    std::size_t riff_size_mismatches = 0;
    std::size_t riff_malformed_records = 0;
    std::size_t riff_zero_trailing_records = 0;
    std::size_t riff_nonzero_trailing_records = 0;
    std::vector<std::string> riff_invalid_examples;
    std::map<std::string, std::size_t> riff_form_types;
    std::map<std::string, std::size_t> riff_chunk_ids;
};

[[nodiscard]] std::string classify_payload_signature(const unsigned char* data, std::size_t size);
[[nodiscard]] PayloadCatalog catalog_payloads(const HeaderSetReport& report);
[[nodiscard]] std::string format_report(const PayloadCatalog& catalog);

} // namespace rb4pc::inspector
