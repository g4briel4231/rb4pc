#ifdef _WIN32

#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <gdiplus.h>
#include <shobjidl.h>
#include <shellapi.h>

#include "rb4pc/builder/pc_asset_catalog.hpp"
#include "rb4pc/builder/pc_audio_catalog.hpp"
#include "rb4pc/builder/pc_character_catalog.hpp"
#include "rb4pc/builder/pc_corpus_audit.hpp"
#include "rb4pc/builder/pc_metadata_catalog.hpp"
#include "rb4pc/inspector/file_inspector.hpp"
#include "rb4pc/inspector/payload_catalog.hpp"
#include "rb4pc/inspector/record_extractor.hpp"

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int id_convert = 1001;
constexpr int id_repair = 1002;
constexpr int id_open_archive = 1003;
constexpr int id_open_folder = 1004;
constexpr int id_exit = 1005;
constexpr int id_open_file_viewer = 1006;
constexpr int id_asset_catalog = 1101;
constexpr int id_audio_catalog = 1102;
constexpr int id_character_catalog = 1103;
constexpr int id_texture_converter = 1104;
constexpr int id_corpus_audit = 1105;
constexpr int id_metadata_catalog = 1106;
constexpr int id_documentation = 1201;
constexpr int id_repository = 1202;
constexpr int id_about = 1203;
constexpr int id_viewer_archives = 1301;
constexpr int id_viewer_records = 1302;
constexpr int id_viewer_search = 1303;
constexpr int id_viewer_details = 1304;
constexpr int id_viewer_extension = 1305;
constexpr int id_viewer_open = 1401;
constexpr int id_viewer_extract = 1402;
constexpr wchar_t file_viewer_class[] = L"RB4PCFileViewerWindow";

struct AppState {
    HWND convert = nullptr;
    HWND repair = nullptr;
    HWND open_archive = nullptr;
    HWND open_folder = nullptr;
    HWND status = nullptr;
    HWND source_label = nullptr;
    HWND file_viewer = nullptr;
    HWND viewer_summary = nullptr;
    HWND viewer_archives = nullptr;
    HWND viewer_records = nullptr;
    HWND viewer_search = nullptr;
    HWND viewer_details = nullptr;
    HWND viewer_extension = nullptr;
    HWND hovered_button = nullptr;
    WNDPROC button_window_proc = nullptr;
    HFONT ui_font = nullptr;
    HFONT button_font = nullptr;
    ULONG_PTR gdiplus_token = 0;
    std::unique_ptr<Gdiplus::Image> logo;
    std::filesystem::path executable_directory;
    std::filesystem::path selected_pc_source;
    std::unique_ptr<rb4pc::inspector::HeaderSetReport> viewer_report;
    std::vector<std::size_t> viewer_record_indices;
    std::optional<std::size_t> viewer_archive_filter;
    std::wstring viewer_extension_filter;
};

AppState app;

std::filesystem::path executable_directory() {
    std::wstring path(32768, L'\0');
    const auto size = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    path.resize(size);
    return std::filesystem::path(path).parent_path();
}

std::filesystem::path find_asset(const std::filesystem::path& relative) {
    for (const auto& candidate : {
             app.executable_directory / relative,
             app.executable_directory / L".." / L".." / relative,
             std::filesystem::current_path() / relative}) {
        std::error_code error;
        if (std::filesystem::is_regular_file(candidate, error)) return candidate;
    }
    return {};
}

void set_status(std::wstring text) {
    SetWindowTextW(app.status, text.c_str());
}

void set_selected_pc_source(const std::filesystem::path& path) {
    app.selected_pc_source = path;
    const std::wstring label = L"Selected PC source: " + path.wstring();
    SetWindowTextW(app.source_label, label.c_str());
}

std::filesystem::path select_file(HWND owner) {
    std::wstring path(32768, L'\0');
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = L"ForgeTool PC files (*_pc;*_pc.*;*_pc_*.ark)\0*_pc;*_pc.*;*_pc_*.ark\0PC HDR files (*_pc.hdr)\0*_pc.hdr\0";
    dialog.lpstrFile = path.data();
    dialog.nMaxFile = static_cast<DWORD>(path.size());
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    if (!GetOpenFileNameW(&dialog)) return {};
    path.resize(std::wcslen(path.c_str()));
    return path;
}

std::filesystem::path select_pc_header(HWND owner) {
    std::wstring path(32768, L'\0');
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = L"PC HDR files (*_pc.hdr)\0*_pc.hdr\0";
    dialog.lpstrFile = path.data();
    dialog.nMaxFile = static_cast<DWORD>(path.size());
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    if (!GetOpenFileNameW(&dialog)) return {};
    path.resize(std::wcslen(path.c_str()));
    return path;
}

std::filesystem::path select_catalog_output(HWND owner, const wchar_t* suggested_name) {
    std::wstring path(32768, L'\0');
    const std::wstring suggested = suggested_name;
    std::copy(suggested.begin(), suggested.end(), path.begin());
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = L"JSON catalog (*.json)\0*.json\0";
    dialog.lpstrFile = path.data();
    dialog.nMaxFile = static_cast<DWORD>(path.size());
    dialog.lpstrDefExt = L"json";
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    if (!GetSaveFileNameW(&dialog)) return {};
    path.resize(std::wcslen(path.c_str()));
    return path;
}

std::filesystem::path select_folder(HWND owner) {
    IFileDialog* dialog = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&dialog)))) return {};
    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    std::filesystem::path result;
    if (SUCCEEDED(dialog->Show(owner))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item))) {
            PWSTR value = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &value))) {
                result = value;
                CoTaskMemFree(value);
            }
            item->Release();
        }
    }
    dialog->Release();
    return result;
}

std::filesystem::path choose_pc_source(HWND window, const wchar_t* title) {
    if (!app.selected_pc_source.empty()) {
        std::wstring prompt = L"Use the selected PC source?\n\n";
        prompt += app.selected_pc_source.wstring();
        prompt += L"\n\nYes: use selected source\nNo: choose another source\nCancel: return";
        const auto choice = MessageBoxW(window, prompt.c_str(), title,
                                        MB_YESNOCANCEL | MB_ICONQUESTION);
        if (choice == IDYES) return app.selected_pc_source;
        if (choice == IDCANCEL) return {};
    }
    const auto choice = MessageBoxW(window,
        L"Choose a PC source.\n\nYes: select one *_pc.hdr file\n"
        L"No: select a folder recursively\nCancel: return",
        title, MB_YESNOCANCEL | MB_ICONQUESTION);
    if (choice == IDCANCEL) return {};
    return choice == IDYES ? select_pc_header(window) : select_folder(window);
}

std::wstring lowercase(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](wchar_t character) { return static_cast<wchar_t>(std::towlower(character)); });
    return value;
}

bool is_forgetool_pc_file(const std::filesystem::path& path) {
    const auto name = lowercase(path.filename().wstring());
    if (name.find(L"_ps4") != std::wstring::npos) return false;
    if (name.ends_with(L"_pc")) return true;
    if (lowercase(path.stem().wstring()).ends_with(L"_pc")) return true;
    return lowercase(path.extension().wstring()) == L".ark" && name.find(L"_pc_") != std::wstring::npos;
}

bool is_pc_header(const std::filesystem::path& path) {
    return lowercase(path.filename().wstring()).ends_with(L"_pc.hdr");
}

void show_pc_error(HWND window, const std::filesystem::path& path, const wchar_t* reason) {
    std::wstring message = L"RB4PC could not open this selection as ForgeTool PC content:\n\n";
    message += path.wstring();
    message += L"\n\n";
    message += reason;
    message += L"\n\nFiles marked _ps4 are intentionally blocked by these buttons.";
    MessageBoxW(window, message.c_str(), L"Unsupported PC content", MB_OK | MB_ICONWARNING);
    set_status(L"PC content was not opened");
}

void open_selected_archive(HWND window) {
    const auto path = select_file(window);
    if (path.empty()) return;
    if (!is_forgetool_pc_file(path)) {
        show_pc_error(window, path, L"Select a ForgeTool output whose name identifies the PC platform (_pc). ");
        return;
    }
    try {
        if (!std::filesystem::is_regular_file(path)) {
            throw std::runtime_error("selection is not a regular file");
        }
        std::wostringstream message;
        message << L"ForgeTool PC file opened successfully.\n\nFile: " << path.filename().wstring()
                << L"\nSize: " << std::filesystem::file_size(path) << L" bytes";
        if (is_pc_header(path)) {
            const auto report = rb4pc::inspector::inspect_header_set(path);
            if (report.platform_hint != "pc" || report.metadata.version != 10 ||
                !report.metadata.hash_index_validated) {
                show_pc_error(window, path, L"The selected _pc.hdr did not pass PC HDR structural validation.");
                return;
            }
            message << L"\nHDR version: " << report.metadata.version
                    << L"\nARK volumes: " << report.archives.size()
                    << L"\nRecords: " << report.metadata.file_records.size();
        }
        MessageBoxW(window, message.str().c_str(), L"ForgeTool PC file opened",
                    MB_OK | MB_ICONINFORMATION);
        set_selected_pc_source(path);
        set_status(L"Opened ForgeTool PC file: " + path.filename().wstring());
    } catch (const std::exception&) {
        show_pc_error(window, path, L"The file could not be read or its PC structure is invalid.");
    }
}

void open_selected_folder(HWND window) {
    const auto path = select_folder(window);
    if (path.empty()) return;
    std::size_t pc_files = 0;
    std::size_t pc_headers = 0;
    std::size_t rejected_headers = 0;
    std::uintmax_t total_bytes = 0;
    std::error_code iterator_error;
    const auto options = std::filesystem::directory_options::skip_permission_denied;
    for (std::filesystem::recursive_directory_iterator iterator(path, options, iterator_error), end;
         iterator != end; iterator.increment(iterator_error)) {
        if (iterator_error) {
            iterator_error.clear();
            continue;
        }
        std::error_code file_error;
        if (!iterator->is_regular_file(file_error) || !is_forgetool_pc_file(iterator->path())) continue;
        if (is_pc_header(iterator->path())) {
            try {
                const auto report = rb4pc::inspector::inspect_header_set(iterator->path());
                if (report.platform_hint != "pc" || report.metadata.version != 10 ||
                    !report.metadata.hash_index_validated) {
                    ++rejected_headers;
                    continue;
                }
                ++pc_headers;
            } catch (const std::exception&) {
                ++rejected_headers;
                continue;
            }
        }
        ++pc_files;
        total_bytes += iterator->file_size(file_error);
    }
    if (pc_files == 0) {
        show_pc_error(window, path, L"The folder contains no readable ForgeTool PC files marked _pc.");
        return;
    }
    std::wostringstream message;
    message << L"ForgeTool PC folder opened successfully.\n\nPC files: " << pc_files
            << L"\nValidated PC HDR sets: " << pc_headers
            << L"\nTotal PC file bytes: " << total_bytes;
    if (rejected_headers) message << L"\nRejected invalid PC HDR files: " << rejected_headers;
    message << L"\n\nFiles marked _ps4 were not loaded.";
    MessageBoxW(window, message.str().c_str(), L"ForgeTool PC folder opened",
                MB_OK | MB_ICONINFORMATION);
    set_selected_pc_source(path);
    set_status(L"Opened ForgeTool PC folder: " + path.wstring());
}

void generate_asset_catalog(HWND window) {
    const auto source = choose_pc_source(window, L"PC Asset Catalog");
    if (source.empty()) return;
    const auto destination = select_catalog_output(window, L"pc-asset-catalog.json");
    if (destination.empty()) return;
    set_status(L"Generating PC asset catalog…");
    try {
        const auto catalog = rb4pc::builder::catalog_pc_assets(source);
        std::ofstream output(destination, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("could not create the selected JSON catalog");
        output << rb4pc::builder::format_json(catalog);
        if (!output) throw std::runtime_error("could not finish the selected JSON catalog");
        std::wostringstream message;
        message << L"PC asset catalog generated.\n\nValid PC HDRs: "
                << catalog.valid_headers << L" / " << catalog.candidate_headers
                << L"\nARK volumes: " << catalog.archive_volumes
                << L"\nRecords: " << catalog.records
                << L"\nNative paths: " << catalog.native_path_records
                << L"\nIntermediate paths: " << catalog.intermediate_path_records
                << L"\nUnknown paths: " << catalog.unknown_path_records
                << L"\n\nOutput: " << destination.wstring();
        MessageBoxW(window, message.str().c_str(), L"Catalog generated",
                    MB_OK | (catalog.invalid_headers ? MB_ICONWARNING : MB_ICONINFORMATION));
        set_status(L"Generated PC asset catalog: " + destination.filename().wstring());
    } catch (const std::exception& error) {
        std::wstring message = L"The PC asset catalog could not be generated.\n\n";
        const std::string detail = error.what();
        message.append(detail.begin(), detail.end());
        MessageBoxW(window, message.c_str(), L"Catalog generation failed", MB_OK | MB_ICONERROR);
        set_status(L"PC asset catalog generation failed");
    }
}

void generate_audio_catalog(HWND window) {
    const auto source = choose_pc_source(window, L"PC Audio Audit");
    if (source.empty()) return;
    const auto destination = select_catalog_output(window, L"pc-audio-audit.json");
    if (destination.empty()) return;
    set_status(L"Generating PC audio audit…");
    try {
        const auto catalog = rb4pc::builder::catalog_pc_audio(source);
        std::ofstream output(destination, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("could not create the selected JSON audit");
        output << rb4pc::builder::format_json(catalog);
        if (!output) throw std::runtime_error("could not finish the selected JSON audit");
        std::wostringstream message;
        message << L"PC audio audit generated.\n\nValid PC HDRs: "
                << catalog.valid_headers << L" / " << catalog.candidate_headers
                << L"\nAudio records: " << catalog.audio_records
                << L"\nMOGG v10 / embedded Ogg: " << catalog.mogg_version_10
                << L" / " << catalog.mogg_embedded_ogg
                << L"\nRIFF WAVE / FEV: " << catalog.riff_wave << L" / " << catalog.riff_fev
                << L"\nUnreadable: " << catalog.unreadable_records
                << L"\n\nOutput: " << destination.wstring();
        MessageBoxW(window, message.str().c_str(), L"Audio audit generated",
                    MB_OK | (catalog.invalid_headers || catalog.unreadable_records
                                 ? MB_ICONWARNING : MB_ICONINFORMATION));
        set_status(L"Generated PC audio audit: " + destination.filename().wstring());
    } catch (const std::exception& error) {
        std::wstring message = L"The PC audio audit could not be generated.\n\n";
        const std::string detail = error.what();
        message.append(detail.begin(), detail.end());
        MessageBoxW(window, message.c_str(), L"Audio audit failed", MB_OK | MB_ICONERROR);
        set_status(L"PC audio audit generation failed");
    }
}

void generate_metadata_catalog(HWND window) {
    const auto source = choose_pc_source(window, L"PC Metadata Catalog");
    if (source.empty()) return;
    const auto destination = select_catalog_output(window, L"pc-metadata-catalog.json");
    if (destination.empty()) return;
    set_status(L"Generating PC metadata catalog…");
    try {
        const auto catalog = rb4pc::builder::catalog_pc_metadata(source);
        std::ofstream output(destination, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("could not create the selected JSON catalog");
        output << rb4pc::builder::format_json(catalog);
        if (!output) throw std::runtime_error("could not finish the selected JSON catalog");
        std::wostringstream message;
        message << L"PC metadata catalog generated.\n\nValid PC HDRs: "
                << catalog.valid_headers << L" / " << catalog.candidate_headers
                << L"\nMetadata records: " << catalog.records
                << L"\nFamilies: " << catalog.families.size()
                << L"\nProfiled fields: " << catalog.profiled_fields()
                << L"\nEvidence classification complete: "
                << (catalog.evidence_classification_complete() ? L"yes" : L"no")
                << L"\nBounded offset fields: " << catalog.bounded_offset_fields()
                << L"\nBounded offset region classification complete: "
                << (catalog.bounded_offset_region_classification_complete() ? L"yes" : L"no")
                << L"\nFully sampled target fields: " << catalog.fully_sampled_target_fields()
                << L"\nSampled target classification complete: "
                << (catalog.sampled_target_classification_complete() ? L"yes" : L"no")
                << L"\nBounded second-hop fields: " << catalog.bounded_second_hop_fields()
                << L"\nSecond-hop region classification complete: "
                << (catalog.second_hop_region_classification_complete() ? L"yes" : L"no")
                << L"\nUnreadable: " << catalog.unreadable_records
                << L"\n\nOutput: " << destination.wstring();
        MessageBoxW(window, message.str().c_str(), L"Metadata catalog generated",
                    MB_OK | (catalog.invalid_headers || catalog.unreadable_records
                                 ? MB_ICONWARNING : MB_ICONINFORMATION));
        set_status(L"Generated PC metadata catalog: " + destination.filename().wstring());
    } catch (const std::exception& error) {
        std::wstring message = L"The PC metadata catalog could not be generated.\n\n";
        const std::string detail = error.what();
        message.append(detail.begin(), detail.end());
        MessageBoxW(window, message.c_str(), L"Metadata catalog failed", MB_OK | MB_ICONERROR);
        set_status(L"PC metadata catalog generation failed");
    }
}

void generate_corpus_audit(HWND window) {
    const auto source = choose_pc_source(window, L"Complete PC Corpus Audit");
    if (source.empty()) return;
    MessageBoxW(window,
        L"Select an existing parent folder. RB4PC will create a new rb4pc-pc-audit folder inside it.",
        L"Select audit destination", MB_OK | MB_ICONINFORMATION);
    const auto parent = select_folder(window);
    if (parent.empty()) return;
    const auto destination = parent / L"rb4pc-pc-audit";
    set_status(L"Generating complete PC corpus audit…");
    try {
        const auto publication = rb4pc::builder::write_pc_corpus_audit(source, destination);
        const auto& audit = publication.audit;
        std::wostringstream message;
        message << L"Complete PC corpus audit generated.\n\nStatus: "
                << (audit.complete() ? L"complete" : L"completed with warnings")
                << L"\nAsset records: " << audit.assets.records
                << L"\nAudio records: " << audit.audio.audio_records
                << L"\nCharacter/animation records: " << audit.characters.records
                << L"\nMetadata records: " << audit.metadata.records
                << L"\nConsistent header counts: " << (audit.consistent_header_counts() ? L"yes" : L"no")
                << L"\nMetadata evidence classification complete: "
                << (audit.metadata.evidence_classification_complete() ? L"yes" : L"no")
                << L"\nMetadata bounded offset regions complete: "
                << (audit.metadata.bounded_offset_region_classification_complete() ? L"yes" : L"no")
                << L"\nMetadata sampled target classification complete: "
                << (audit.metadata.sampled_target_classification_complete() ? L"yes" : L"no")
                << L"\nMetadata second-hop regions complete: "
                << (audit.metadata.second_hop_region_classification_complete() ? L"yes" : L"no")
                << L"\n\nFiles: summary.json, assets.json, audio.json, characters.json, metadata.json"
                << L"\nOutput: " << publication.directory.wstring();
        MessageBoxW(window, message.str().c_str(), L"PC corpus audit generated",
                    MB_OK | (audit.complete() ? MB_ICONINFORMATION : MB_ICONWARNING));
        set_status(L"Generated complete PC corpus audit");
    } catch (const std::exception& error) {
        std::wstring message = L"The complete PC corpus audit could not be generated.\n\n";
        const std::string detail = error.what();
        message.append(detail.begin(), detail.end());
        MessageBoxW(window, message.c_str(), L"PC corpus audit failed", MB_OK | MB_ICONERROR);
        set_status(L"Complete PC corpus audit failed");
    }
}

void generate_character_catalog(HWND window) {
    const auto source = choose_pc_source(window, L"Character/Animation Structural Profile");
    if (source.empty()) return;
    const auto destination = select_catalog_output(window, L"pc-character-catalog.json");
    if (destination.empty()) return;
    set_status(L"Generating character/animation structural profile…");
    try {
        const auto catalog = rb4pc::builder::catalog_pc_characters(source);
        std::ofstream output(destination, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("could not create the selected JSON catalog");
        output << rb4pc::builder::format_json(catalog);
        if (!output) throw std::runtime_error("could not finish the selected JSON catalog");
        std::wostringstream message;
        message << L"Character/animation structural profile generated.\n\n"
                << L"Valid PC HDRs: " << catalog.valid_headers << L" / " << catalog.candidate_headers
                << L"\nRecords: " << catalog.records
                << L"\nFamilies: " << catalog.families.size()
                << L"\nUnreadable records: " << catalog.unreadable_records
                << L"\n\nOutput: " << destination.wstring();
        MessageBoxW(window, message.str().c_str(), L"Catalog generated",
                    MB_OK | (catalog.invalid_headers || catalog.unreadable_records
                                 ? MB_ICONWARNING : MB_ICONINFORMATION));
        set_status(L"Generated character catalog: " + destination.filename().wstring());
    } catch (const std::exception& error) {
        std::wstring message = L"The character/animation catalog could not be generated.\n\n";
        const std::string detail = error.what();
        message.append(detail.begin(), detail.end());
        MessageBoxW(window, message.c_str(), L"Catalog generation failed", MB_OK | MB_ICONERROR);
        set_status(L"Character catalog generation failed");
    }
}

void show_planned(HWND window, const wchar_t* title, const wchar_t* detail) {
    MessageBoxW(window, detail, title, MB_OK | MB_ICONINFORMATION);
    set_status(std::wstring(title) + L" — wizard planned for a future milestone");
}

void show_tool(HWND window, const wchar_t* name, const wchar_t* command) {
    std::wstring message = L"This tool is already available through RB4PC Builder's command-line core:\n\n";
    message += command;
    message += L"\n\nA dedicated GUI workflow will be connected in the next Builder GUI milestones.";
    MessageBoxW(window, message.c_str(), name, MB_OK | MB_ICONINFORMATION);
    set_status(std::wstring(name) + L" — available in the command-line core");
}

void open_path_or_url(const std::wstring& value) {
    ShellExecuteW(nullptr, L"open", value.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

std::wstring utf8_to_wide(const std::string& value) {
    if (value.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, value.data(),
                                         static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) return std::wstring(value.begin(), value.end());
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                        result.data(), size);
    return result;
}

std::optional<std::size_t> selected_viewer_record_index() {
    if (!app.viewer_records) return std::nullopt;
    const auto selection = SendMessageW(app.viewer_records, LB_GETCURSEL, 0, 0);
    if (selection == LB_ERR || static_cast<std::size_t>(selection) >= app.viewer_record_indices.size()) return std::nullopt;
    return app.viewer_record_indices[static_cast<std::size_t>(selection)];
}

std::wstring viewer_record_details(std::size_t index) {
    if (!app.viewer_report || index >= app.viewer_report->metadata.file_records.size()) return L"No file selected";
    const auto& report = *app.viewer_report;
    const auto& record = report.metadata.file_records[index];
    std::wostringstream detail;
    detail << L"Path: " << utf8_to_wide(record.path)
           << L"\r\nSize: " << record.size << L" bytes"
           << L"    Global offset: " << record.global_offset;
    if (index < report.record_locations.size() && report.record_locations[index]) {
        detail << L"    ARK: " << report.record_locations[index]->archive_index
               << L"    Local offset: " << report.record_locations[index]->local_offset;
    } else {
        detail << L"    ARK location: unresolved";
    }
    return detail.str();
}

void update_viewer_record_details() {
    const auto index = selected_viewer_record_index();
    const auto text = index ? viewer_record_details(*index) : L"No file selected";
    SetWindowTextW(app.viewer_details, text.c_str());
}

void open_selected_viewer_record(HWND window) {
    const auto index = selected_viewer_record_index();
    if (!index || !app.viewer_report) return;
    const auto& record = app.viewer_report->metadata.file_records[*index];
    std::wstring detail = viewer_record_details(*index);
    try {
        const auto preview = rb4pc::inspector::inspect_record(*app.viewer_report, record.path);
        if (preview) {
            const auto* bytes = reinterpret_cast<const unsigned char*>(preview->bytes.data());
            const auto signature = rb4pc::inspector::classify_payload_signature(bytes, preview->size);
            std::wostringstream preview_text;
            preview_text << L"\r\nDetected type: " << utf8_to_wide(signature) << L"\r\nPreview: ";
            preview_text << std::hex << std::setfill(L'0');
            for (std::size_t offset = 0; offset < preview->size; ++offset) {
                if (offset) preview_text << L' ';
                preview_text << std::setw(2) << static_cast<unsigned int>(bytes[offset]);
            }
            detail += preview_text.str();
        } else {
            detail += L"\r\nPreview unavailable: record could not be resolved.";
        }
    } catch (const std::exception&) {
        detail += L"\r\nPreview unavailable: the ARK data could not be read.";
    }
    MessageBoxW(window, detail.c_str(), L"Open — File Viewer", MB_OK | MB_ICONINFORMATION);
}

std::filesystem::path select_extraction_destination(HWND owner, const std::string& internal_path) {
    std::wstring path(32768, L'\0');
    const auto suggested = utf8_to_wide(std::filesystem::path(internal_path).filename().string());
    std::copy_n(suggested.begin(), std::min(suggested.size(), path.size() - 1), path.begin());
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = L"All files\0*.*\0";
    dialog.lpstrFile = path.data();
    dialog.nMaxFile = static_cast<DWORD>(path.size());
    dialog.Flags = OFN_PATHMUSTEXIST | OFN_EXPLORER;
    if (!GetSaveFileNameW(&dialog)) return {};
    path.resize(std::wcslen(path.c_str()));
    if (std::filesystem::exists(path)) {
        MessageBoxW(owner, L"Choose a new filename. File Viewer never overwrites an existing file.",
                    L"Extract — File Viewer", MB_OK | MB_ICONWARNING);
        return {};
    }
    return path;
}

void extract_selected_viewer_record(HWND window) {
    const auto index = selected_viewer_record_index();
    if (!index || !app.viewer_report) return;
    const auto& record = app.viewer_report->metadata.file_records[*index];
    const auto destination = select_extraction_destination(window, record.path);
    if (destination.empty()) return;
    try {
        const auto result = rb4pc::inspector::extract_record(*app.viewer_report, record.path, destination);
        std::wostringstream message;
        message << L"File extracted successfully.\n\nPath: " << result.destination.wstring()
                << L"\nSize: " << result.size << L" bytes"
                << L"\nSHA-256: " << utf8_to_wide(result.sha256);
        MessageBoxW(window, message.str().c_str(), L"Extract — File Viewer",
                    MB_OK | MB_ICONINFORMATION);
    } catch (const std::exception& error) {
        std::wstring message = L"The selected file could not be extracted.\n\n";
        const std::string detail = error.what();
        message.append(detail.begin(), detail.end());
        MessageBoxW(window, message.c_str(), L"Extract failed", MB_OK | MB_ICONERROR);
    }
}

void populate_file_viewer_records(std::optional<std::size_t> archive_filter) {
    if (!app.viewer_records || !app.viewer_report) return;
    app.viewer_archive_filter = archive_filter;
    const auto& report = *app.viewer_report;
    std::wstring query;
    if (app.viewer_search) {
        const int length = GetWindowTextLengthW(app.viewer_search);
        query.resize(static_cast<std::size_t>(std::max(0, length)));
        if (length > 0) GetWindowTextW(app.viewer_search, query.data(), length + 1);
        query = lowercase(std::move(query));
    }
    app.viewer_record_indices.clear();
    for (std::size_t index = 0; index < report.metadata.file_records.size(); ++index) {
        const auto has_location = index < report.record_locations.size() &&
                                  report.record_locations[index].has_value();
        if (archive_filter && (!has_location ||
            report.record_locations[index]->archive_index != *archive_filter)) continue;
        const auto wide_path = lowercase(utf8_to_wide(report.metadata.file_records[index].path));
        if (!query.empty() && wide_path.find(query) == std::wstring::npos) continue;
        if (!app.viewer_extension_filter.empty() &&
            lowercase(std::filesystem::path(wide_path).extension().wstring()) != app.viewer_extension_filter) continue;
        app.viewer_record_indices.push_back(index);
    }
    std::sort(app.viewer_record_indices.begin(), app.viewer_record_indices.end(),
        [&report](std::size_t left, std::size_t right) {
            return lowercase(utf8_to_wide(report.metadata.file_records[left].path)) <
                   lowercase(utf8_to_wide(report.metadata.file_records[right].path));
        });
    SendMessageW(app.viewer_records, LB_RESETCONTENT, 0, 0);
    for (const auto index : app.viewer_record_indices) {
        const auto& record = report.metadata.file_records[index];
        const auto has_location = index < report.record_locations.size() &&
                                  report.record_locations[index].has_value();
        std::wostringstream line;
        if (has_location) {
            line << L"[ARK " << report.record_locations[index]->archive_index
                 << L" @ " << report.record_locations[index]->local_offset << L"] ";
        } else {
            line << L"[unresolved] ";
        }
        line << utf8_to_wide(record.path) << L"  (" << record.size << L" bytes)";
        const auto value = line.str();
        SendMessageW(app.viewer_records, LB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(value.c_str()));
    }
    std::wostringstream summary;
    summary << L"HDR: " << report.header.path.filename().wstring()
            << L"    ARKs: " << report.archives.size()
            << L"    Visible files: " << app.viewer_record_indices.size()
            << L" / " << report.metadata.file_records.size();
    if (!app.viewer_extension_filter.empty()) summary << L"    Extension: " << app.viewer_extension_filter;
    if (archive_filter) summary << L"    Filter: ARK " << *archive_filter;
    else summary << L"    Filter: all ARKs";
    SetWindowTextW(app.viewer_summary, summary.str().c_str());
}
void populate_file_viewer() {
    if (!app.file_viewer || !app.viewer_report) return;
    const auto& report = *app.viewer_report;
    std::wostringstream summary;
    summary << L"HDR: " << report.header.path.filename().wstring()
            << L"    Version: " << report.metadata.version
            << L"    ARK volumes: " << report.archives.size()
            << L"    Files: " << report.metadata.file_records.size();
    SetWindowTextW(app.viewer_summary, summary.str().c_str());
    SendMessageW(app.viewer_extension, CB_RESETCONTENT, 0, 0);
    SendMessageW(app.viewer_extension, CB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(L"All extensions"));
    std::vector<std::wstring> extensions;
    for (const auto& record : report.metadata.file_records) {
        auto extension = lowercase(std::filesystem::path(utf8_to_wide(record.path)).extension().wstring());
        if (!extension.empty() && std::find(extensions.begin(), extensions.end(), extension) == extensions.end())
            extensions.push_back(std::move(extension));
    }
    std::sort(extensions.begin(), extensions.end());
    for (const auto& extension : extensions)
        SendMessageW(app.viewer_extension, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(extension.c_str()));
    SendMessageW(app.viewer_extension, CB_SETCURSEL, 0, 0);
    app.viewer_extension_filter.clear();
    SendMessageW(app.viewer_archives, LB_RESETCONTENT, 0, 0);
    SendMessageW(app.viewer_archives, LB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(L"All ARK volumes"));
    for (std::size_t index = 0; index < report.archives.size(); ++index) {
        const auto& archive = report.archives[index];
        std::wostringstream line;
        line << L"ARK " << index << L": " << archive.path.filename().wstring()
             << L"  [" << (archive.exists ? L"present" : L"missing") << L"]  "
             << archive.actual_size << L" / " << archive.declared_size << L" bytes";
        const auto value = line.str();
        SendMessageW(app.viewer_archives, LB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(value.c_str()));
    }
    SendMessageW(app.viewer_archives, LB_SETCURSEL, 0, 0);
    populate_file_viewer_records(std::nullopt);
}

void layout_file_viewer(HWND window) {
    RECT client{};
    GetClientRect(window, &client);
    const int margin = 12;
    const int gap = 10;
    const int summary_height = 28;
    const int search_height = 26;
    const int content_y = margin + summary_height + gap + search_height + gap;
    const int details_height = 72;
    const int content_height = std::max(80, static_cast<int>(client.bottom) - content_y - margin - gap - details_height);
    const int archive_width = std::max(220, (static_cast<int>(client.right) - margin * 2 - gap) / 3);
    MoveWindow(app.viewer_summary, margin, margin, client.right - margin * 2,
               summary_height, TRUE);
    const int extension_width = 210;
    MoveWindow(app.viewer_search, margin, margin + summary_height + gap,
               client.right - margin * 2 - gap - extension_width, search_height, TRUE);
    MoveWindow(app.viewer_extension, client.right - margin - extension_width,
               margin + summary_height + gap, extension_width, 240, TRUE);
    MoveWindow(app.viewer_archives, margin, content_y, archive_width, content_height, TRUE);
    MoveWindow(app.viewer_records, margin + archive_width + gap, content_y,
               client.right - margin * 2 - archive_width - gap, content_height, TRUE);
    MoveWindow(app.viewer_details, margin, content_y + content_height + gap,
               client.right - margin * 2, details_height, TRUE);
}

HMENU create_file_viewer_menu() {
    const auto menu = CreateMenu();
    const auto tools = CreatePopupMenu();
    AppendMenuW(tools, MF_STRING, id_asset_catalog, L"PC Asset Catalog…");
    AppendMenuW(tools, MF_STRING, id_audio_catalog, L"PC Audio Audit…");
    AppendMenuW(tools, MF_STRING, id_character_catalog, L"Character/Animation Inventory…");
    AppendMenuW(tools, MF_STRING, id_metadata_catalog, L"PC Metadata Catalog…");
    AppendMenuW(tools, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(tools, MF_STRING, id_corpus_audit, L"Complete PC Corpus Audit…");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(tools), L"Tools");
    return menu;
}

LRESULT CALLBACK file_viewer_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_CREATE:
            app.viewer_summary = CreateWindowExW(0, L"STATIC", L"No HDR loaded",
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE | SS_PATHELLIPSIS,
                0, 0, 100, 28, window, nullptr, GetModuleHandleW(nullptr), nullptr);
            app.viewer_search = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                0, 0, 100, 26, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id_viewer_search)),
                GetModuleHandleW(nullptr), nullptr);
            app.viewer_extension = CreateWindowExW(0, L"COMBOBOX", nullptr,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                0, 0, 100, 240, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id_viewer_extension)),
                GetModuleHandleW(nullptr), nullptr);
            app.viewer_archives = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", nullptr,
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | LBS_NOINTEGRALHEIGHT | LBS_NOTIFY,
                0, 0, 100, 100, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id_viewer_archives)),
                GetModuleHandleW(nullptr), nullptr);
            app.viewer_records = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", nullptr,
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | LBS_NOINTEGRALHEIGHT | LBS_NOTIFY,
                0, 0, 100, 100, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id_viewer_records)),
                GetModuleHandleW(nullptr), nullptr);
            app.viewer_details = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"No file selected",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                0, 0, 100, 72, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id_viewer_details)),
                GetModuleHandleW(nullptr), nullptr);
            SendMessageW(app.viewer_summary, WM_SETFONT, reinterpret_cast<WPARAM>(app.ui_font), TRUE);
            SendMessageW(app.viewer_search, WM_SETFONT, reinterpret_cast<WPARAM>(app.ui_font), TRUE);
            SendMessageW(app.viewer_extension, WM_SETFONT, reinterpret_cast<WPARAM>(app.ui_font), TRUE);
            SendMessageW(app.viewer_archives, WM_SETFONT, reinterpret_cast<WPARAM>(app.ui_font), TRUE);
            SendMessageW(app.viewer_records, WM_SETFONT, reinterpret_cast<WPARAM>(app.ui_font), TRUE);
            SendMessageW(app.viewer_details, WM_SETFONT, reinterpret_cast<WPARAM>(app.ui_font), TRUE);
            layout_file_viewer(window);
            return 0;
        case WM_SIZE: layout_file_viewer(window); return 0;
        case WM_CONTEXTMENU:
            if (reinterpret_cast<HWND>(wparam) == app.viewer_records) {
                const auto index = selected_viewer_record_index();
                const auto menu = CreatePopupMenu();
                AppendMenuW(menu, MF_STRING | (index ? MF_ENABLED : MF_GRAYED), id_viewer_open, L"Open");
                AppendMenuW(menu, MF_STRING | (index ? MF_ENABLED : MF_GRAYED), id_viewer_extract, L"Extract…");
                const int x = GET_X_LPARAM(lparam);
                const int y = GET_Y_LPARAM(lparam);
                TrackPopupMenu(menu, TPM_RIGHTBUTTON, x, y, 0, window, nullptr);
                DestroyMenu(menu);
                return 0;
            }
            break;
        case WM_COMMAND:
            switch (LOWORD(wparam)) {
                case id_asset_catalog: generate_asset_catalog(window); return 0;
                case id_audio_catalog: generate_audio_catalog(window); return 0;
                case id_character_catalog: generate_character_catalog(window); return 0;
                case id_metadata_catalog: generate_metadata_catalog(window); return 0;
                case id_corpus_audit: generate_corpus_audit(window); return 0;
            }
            if (LOWORD(wparam) == id_viewer_extract) {
                extract_selected_viewer_record(window);
                return 0;
            }
            if (LOWORD(wparam) == id_viewer_open) {
                open_selected_viewer_record(window);
                return 0;
            }
            if (LOWORD(wparam) == id_viewer_records && HIWORD(wparam) == LBN_DBLCLK) {
                open_selected_viewer_record(window);
                return 0;
            }
            if (LOWORD(wparam) == id_viewer_records && HIWORD(wparam) == LBN_SELCHANGE) {
                update_viewer_record_details();
                return 0;
            }
            if (LOWORD(wparam) == id_viewer_extension && HIWORD(wparam) == CBN_SELCHANGE) {
                const auto selection = SendMessageW(app.viewer_extension, CB_GETCURSEL, 0, 0);
                app.viewer_extension_filter.clear();
                if (selection > 0) {
                    const auto length = SendMessageW(app.viewer_extension, CB_GETLBTEXTLEN, selection, 0);
                    app.viewer_extension_filter.resize(static_cast<std::size_t>(length));
                    SendMessageW(app.viewer_extension, CB_GETLBTEXT, selection,
                                 reinterpret_cast<LPARAM>(app.viewer_extension_filter.data()));
                }
                populate_file_viewer_records(app.viewer_archive_filter);
                return 0;
            }
            if (LOWORD(wparam) == id_viewer_search && HIWORD(wparam) == EN_CHANGE) {
                populate_file_viewer_records(app.viewer_archive_filter);
                return 0;
            }
            if (LOWORD(wparam) == id_viewer_archives && HIWORD(wparam) == LBN_SELCHANGE) {
                const auto selection = SendMessageW(app.viewer_archives, LB_GETCURSEL, 0, 0);
                if (selection <= 0) populate_file_viewer_records(std::nullopt);
                else populate_file_viewer_records(static_cast<std::size_t>(selection - 1));
                return 0;
            }
            break;
        case WM_GETMINMAXINFO: {
            auto* limits = reinterpret_cast<MINMAXINFO*>(lparam);
            limits->ptMinTrackSize.x = 760;
            limits->ptMinTrackSize.y = 420;
            return 0;
        }
        case WM_DESTROY:
            app.file_viewer = nullptr;
            app.viewer_summary = nullptr;
            app.viewer_archives = nullptr;
            app.viewer_records = nullptr;
            app.viewer_search = nullptr;
            app.viewer_details = nullptr;
            app.viewer_extension = nullptr;
            app.viewer_extension_filter.clear();
            app.viewer_record_indices.clear();
            app.viewer_archive_filter.reset();
            return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

void open_file_viewer(HWND owner, const std::filesystem::path& header) {
    try {
        auto report = rb4pc::inspector::inspect_header_set(header);
        if (report.platform_hint != "pc" || report.metadata.version != 10 ||
            !report.metadata.hash_index_validated) {
            show_pc_error(owner, header, L"The dropped HDR did not pass PC HDR structural validation.");
            return;
        }
        app.viewer_report = std::make_unique<rb4pc::inspector::HeaderSetReport>(std::move(report));
        if (!app.file_viewer) {
            app.file_viewer = CreateWindowExW(0, file_viewer_class, L"File Viewer",
                WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 1080, 640,
                owner, create_file_viewer_menu(), GetModuleHandleW(nullptr), nullptr);
            if (!app.file_viewer) throw std::runtime_error("could not create File Viewer window");
        } else {
            ShowWindow(app.file_viewer, SW_RESTORE);
            SetForegroundWindow(app.file_viewer);
        }
        set_selected_pc_source(header);
        populate_file_viewer();
        set_status(L"Opened PC HDR in File Viewer: " + header.filename().wstring());
    } catch (const std::exception&) {
        show_pc_error(owner, header, L"The HDR and its ARK set could not be opened in File Viewer.");
    }
}

HMENU create_menu() {
    const auto menu = CreateMenu();
    const auto file = CreatePopupMenu();
    AppendMenuW(file, MF_STRING, id_open_archive, L"Open Package or Archive…");
    AppendMenuW(file, MF_STRING, id_open_folder, L"Open Folder…");
    AppendMenuW(file, MF_STRING, id_open_file_viewer, L"Open PC HDR in File Viewer…");
    AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, id_exit, L"Exit");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(file), L"File");

    const auto tools = CreatePopupMenu();
    AppendMenuW(tools, MF_STRING, id_asset_catalog, L"PC Asset Catalog…");
    AppendMenuW(tools, MF_STRING, id_audio_catalog, L"PC Audio Audit…");
    AppendMenuW(tools, MF_STRING, id_character_catalog, L"Character/Animation Inventory…");
    AppendMenuW(tools, MF_STRING, id_metadata_catalog, L"PC Metadata Catalog…");
    AppendMenuW(tools, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(tools, MF_STRING, id_corpus_audit, L"Complete PC Corpus Audit…");
    AppendMenuW(tools, MF_STRING, id_texture_converter, L"Texture Converter…");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(tools), L"Tools");

    const auto help = CreatePopupMenu();
    AppendMenuW(help, MF_STRING, id_documentation, L"Documentation");
    AppendMenuW(help, MF_STRING, id_repository, L"GitHub Repository");
    AppendMenuW(help, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(help, MF_STRING, id_about, L"About RB4PC Builder");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(help), L"Help");
    return menu;
}

LRESULT CALLBACK button_subclass_proc(HWND button, UINT message, WPARAM wparam,
                                      LPARAM lparam) {
    switch (message) {
        case WM_MOUSEMOVE:
            if (app.hovered_button != button) {
                const auto previous = app.hovered_button;
                app.hovered_button = button;
                if (previous) InvalidateRect(previous, nullptr, TRUE);
                InvalidateRect(button, nullptr, TRUE);
                TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, button, 0};
                TrackMouseEvent(&tracking);
            }
            break;
        case WM_MOUSELEAVE:
            if (app.hovered_button == button) app.hovered_button = nullptr;
            InvalidateRect(button, nullptr, TRUE);
            break;
        case WM_SETFOCUS:
        case WM_KILLFOCUS:
        case WM_ENABLE:
            InvalidateRect(button, nullptr, TRUE);
            break;
    }
    return CallWindowProcW(app.button_window_proc, button, message, wparam, lparam);
}

void draw_button(const DRAWITEMSTRUCT& item) {
    RECT bounds = item.rcItem;
    FillRect(item.hDC, &bounds, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));

    const bool disabled = (item.itemState & ODS_DISABLED) != 0;
    const bool pressed = (item.itemState & ODS_SELECTED) != 0;
    const bool focused = (item.itemState & ODS_FOCUS) != 0;
    const bool hovered = app.hovered_button == item.hwndItem;
    HIGHCONTRASTW contrast{sizeof(contrast)};
    const bool high_contrast = SystemParametersInfoW(
        SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0) &&
        (contrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
    const bool interactive = hovered || focused;
    const COLORREF fill = high_contrast
        ? GetSysColor(pressed ? COLOR_HIGHLIGHT : COLOR_BTNFACE)
        : disabled ? RGB(247, 247, 247)
        : pressed ? RGB(229, 240, 250)
        : interactive ? RGB(245, 250, 255)
                      : RGB(252, 252, 252);
    const COLORREF border = high_contrast
        ? GetSysColor((interactive || pressed) ? COLOR_HIGHLIGHT : COLOR_BTNTEXT)
        : interactive ? GetSysColor(COLOR_HIGHLIGHT)
                      : RGB(204, 204, 204);
    const COLORREF text = high_contrast
        ? GetSysColor(pressed ? COLOR_HIGHLIGHTTEXT
                              : disabled ? COLOR_GRAYTEXT : COLOR_BTNTEXT)
        : disabled ? RGB(150, 150, 150) : RGB(30, 30, 30);

    InflateRect(&bounds, -1, -1);
    const auto brush = CreateSolidBrush(fill);
    const auto pen = CreatePen(PS_SOLID, 1, border);
    const auto old_brush = SelectObject(item.hDC, brush);
    const auto old_pen = SelectObject(item.hDC, pen);
    RoundRect(item.hDC, bounds.left, bounds.top, bounds.right, bounds.bottom, 6, 6);
    SelectObject(item.hDC, old_pen);
    SelectObject(item.hDC, old_brush);
    DeleteObject(pen);
    DeleteObject(brush);

    wchar_t label[256]{};
    GetWindowTextW(item.hwndItem, label, static_cast<int>(std::size(label)));
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, text);
    const auto old_font = SelectObject(item.hDC, app.button_font);
    RECT text_bounds = bounds;
    if (pressed) OffsetRect(&text_bounds, 0, 1);
    DrawTextW(item.hDC, label, -1, &text_bounds,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(item.hDC, old_font);
}

HWND create_button(HWND parent, int id, const wchar_t* text) {
    const auto button = CreateWindowExW(0, L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        0, 0, 100, 40, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr), nullptr);
    SendMessageW(button, WM_SETFONT, reinterpret_cast<WPARAM>(app.button_font), TRUE);
    const auto original_proc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
        button, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(button_subclass_proc)));
    if (!app.button_window_proc) app.button_window_proc = original_proc;
    return button;
}

void layout(HWND window) {
    RECT client{};
    GetClientRect(window, &client);
    const int width = client.right;
    const int height = client.bottom;
    const int margin = 40;
    const int gap = 12;
    const int status_height = 28;
    const int first_y = 220;
    const int button_height = 78;
    const int preferred_width = 720;
    const int full_width = std::max(100, std::min(preferred_width, width - margin * 2));
    const int content_x = (width - full_width) / 2;
    MoveWindow(app.source_label, content_x, first_y - 30, full_width, 22, TRUE);
    MoveWindow(app.convert, content_x, first_y, full_width, button_height, TRUE);
    MoveWindow(app.repair, content_x, first_y + button_height + gap, full_width, button_height, TRUE);
    const int half_width = (full_width - gap) / 2;
    const int third_y = first_y + (button_height + gap) * 2;
    MoveWindow(app.open_archive, content_x, third_y, half_width, button_height, TRUE);
    MoveWindow(app.open_folder, content_x + half_width + gap, third_y,
               full_width - half_width - gap, button_height, TRUE);
    MoveWindow(app.status, 0, std::max(third_y + button_height + 12, height - status_height),
               width, status_height, TRUE);
}

void paint_logo(HWND window) {
    PAINTSTRUCT paint{};
    const auto dc = BeginPaint(window, &paint);
    RECT client{};
    GetClientRect(window, &client);
    FillRect(dc, &client, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
    if (app.logo && app.logo->GetLastStatus() == Gdiplus::Ok) {
        const int area_x = 80;
        const int area_y = 25;
        const int area_width = std::max(1, static_cast<int>(client.right) - area_x * 2);
        const int area_height = 165;
        const auto image_width = static_cast<double>(app.logo->GetWidth());
        const auto image_height = static_cast<double>(app.logo->GetHeight());
        const auto scale = std::min(area_width / image_width, area_height / image_height);
        const int width = static_cast<int>(image_width * scale);
        const int height = static_cast<int>(image_height * scale);
        Gdiplus::Graphics graphics(dc);
        graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        graphics.DrawImage(app.logo.get(), (client.right - width) / 2,
                           area_y + (area_height - height) / 2, width, height);
    } else {
        SetBkMode(dc, TRANSPARENT);
        SelectObject(dc, app.ui_font);
        RECT title{40, 60, client.right - 40, 150};
        DrawTextW(dc, L"RB4PC", -1, &title, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    EndPaint(window, &paint);
}

int font_height_for_tenths_of_point(HWND window, int tenths_of_point) {
    const auto dc = GetDC(window);
    const int dpi = dc ? GetDeviceCaps(dc, LOGPIXELSY) : 96;
    if (dc) ReleaseDC(window, dc);
    return -MulDiv(tenths_of_point, dpi, 720);
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_CREATE: {
            app.ui_font = CreateFontW(font_height_for_tenths_of_point(window, 120), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            app.button_font = CreateFontW(font_height_for_tenths_of_point(window, 135), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                         CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            app.source_label = CreateWindowExW(0, L"STATIC", L"Selected PC source: none",
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE | SS_PATHELLIPSIS,
                0, 0, 100, 22, window, nullptr, GetModuleHandleW(nullptr), nullptr);
            SendMessageW(app.source_label, WM_SETFONT, reinterpret_cast<WPARAM>(app.ui_font), TRUE);
            app.convert = create_button(window, id_convert, L"Convert Rock Band 4 PS4 to PC");
            app.repair = create_button(window, id_repair, L"Repair Rock Band 4 PC");
            app.open_archive = create_button(window, id_open_archive, L"Open a ForgeTool PC File…");
            app.open_folder = create_button(window, id_open_folder, L"Open a Folder…");
            DragAcceptFiles(window, TRUE);
            app.status = CreateWindowExW(0, L"STATIC", L"  Ready — RB4PC Builder v" RB4PC_VERSION_W,
                WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE | SS_SUNKEN, 0, 0, 100, 28, window,
                nullptr, GetModuleHandleW(nullptr), nullptr);
            SendMessageW(app.status, WM_SETFONT, reinterpret_cast<WPARAM>(app.ui_font), TRUE);
            layout(window);
            return 0;
        }
        case WM_SIZE: layout(window); return 0;
        case WM_GETMINMAXINFO: {
            auto* limits = reinterpret_cast<MINMAXINFO*>(lparam);
            limits->ptMinTrackSize.x = 720;
            limits->ptMinTrackSize.y = 560;
            return 0;
        }
        case WM_SYSCOLORCHANGE:
        case WM_SETTINGCHANGE:
            InvalidateRect(app.convert, nullptr, TRUE);
            InvalidateRect(app.repair, nullptr, TRUE);
            InvalidateRect(app.open_archive, nullptr, TRUE);
            InvalidateRect(app.open_folder, nullptr, TRUE);
            InvalidateRect(window, nullptr, TRUE);
            break;
        case WM_PAINT: paint_logo(window); return 0;
        case WM_DRAWITEM:
            if (wparam == id_convert || wparam == id_repair ||
                wparam == id_open_archive || wparam == id_open_folder) {
                draw_button(*reinterpret_cast<DRAWITEMSTRUCT*>(lparam));
                return TRUE;
            }
            break;
        case WM_DROPFILES: {
            const auto drop = reinterpret_cast<HDROP>(wparam);
            const UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
            if (count != 1) {
                DragFinish(drop);
                MessageBoxW(window, L"Drop one validated *_pc.hdr file at a time.",
                            L"File Viewer", MB_OK | MB_ICONWARNING);
                return 0;
            }
            const UINT length = DragQueryFileW(drop, 0, nullptr, 0);
            std::wstring value(static_cast<std::size_t>(length) + 1, L'\0');
            DragQueryFileW(drop, 0, value.data(), length + 1);
            DragFinish(drop);
            value.resize(length);
            const std::filesystem::path path(value);
            if (!is_pc_header(path) || !std::filesystem::is_regular_file(path)) {
                show_pc_error(window, path, L"File Viewer accepts one validated *_pc.hdr file.");
                return 0;
            }
            open_file_viewer(window, path);
            return 0;
        }
        case WM_COMMAND:
            switch (LOWORD(wparam)) {
                case id_convert:
                    show_planned(window, L"Convert Rock Band 4 PS4 to PC",
                        L"The full conversion wizard will be connected after native metadata, RBMID, and remaining asset formats are implemented.\n\nCurrent HDR/ARK generation and validated asset tools remain available in the command-line core.");
                    return 0;
                case id_repair:
                    show_planned(window, L"Repair Rock Band 4 PC",
                        L"Repair requires a complete native-PC manifest and is reserved for a later milestone.");
                    return 0;
                case id_open_archive: open_selected_archive(window); return 0;
                case id_open_folder: open_selected_folder(window); return 0;
                case id_open_file_viewer: {
                    const auto header = select_pc_header(window);
                    if (!header.empty()) open_file_viewer(window, header);
                    return 0;
                }
                case id_asset_catalog: generate_asset_catalog(window); return 0;
                case id_audio_catalog: generate_audio_catalog(window); return 0;
                case id_character_catalog: generate_character_catalog(window); return 0;
                case id_corpus_audit: generate_corpus_audit(window); return 0;
                case id_metadata_catalog: generate_metadata_catalog(window); return 0;
                case id_texture_converter: show_tool(window, L"Texture Converter", L"rb4pc-texture-probe --convert-pc"); return 0;
                case id_documentation: {
                    const auto docs = find_asset(L"docs");
                    open_path_or_url(docs.empty() ? L"https://github.com/rb4pc/rb4pc-builder/wiki" : docs.wstring());
                    return 0;
                }
                case id_repository: open_path_or_url(L"https://github.com/rb4pc/rb4pc-builder"); return 0;
                case id_about:
                    MessageBoxW(window,
                        L"RB4PC Builder v" RB4PC_VERSION_W L"\n\nIndependent clean-room tooling for user-supplied Rock Band 4 assets.\nNo proprietary game content is included.",
                        L"About RB4PC Builder", MB_OK | MB_ICONINFORMATION);
                    return 0;
                case id_exit: DestroyWindow(window); return 0;
            }
            break;
        case WM_DESTROY:
            if (app.button_font) DeleteObject(app.button_font);
            if (app.ui_font) DeleteObject(app.ui_font);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    SetProcessDPIAware();
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 1;
    app.executable_directory = executable_directory();
    Gdiplus::GdiplusStartupInput gdiplus_input;
    if (Gdiplus::GdiplusStartup(&app.gdiplus_token, &gdiplus_input, nullptr) == Gdiplus::Ok) {
        const auto logo = find_asset(L"assets/branding/rb4pc-logo1.png");
        if (!logo.empty()) app.logo = std::make_unique<Gdiplus::Image>(logo.c_str());
    }

    WNDCLASSEXW klass{};
    klass.cbSize = sizeof(klass);
    klass.style = CS_HREDRAW | CS_VREDRAW;
    klass.hInstance = instance;
    klass.lpfnWndProc = window_proc;
    klass.lpszClassName = L"RB4PCBuilderWindow";
    klass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    klass.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    klass.hIconSm = klass.hIcon;
    klass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    if (!RegisterClassExW(&klass)) return 1;
    WNDCLASSEXW viewer_class = klass;
    viewer_class.lpfnWndProc = file_viewer_proc;
    viewer_class.lpszClassName = file_viewer_class;
    if (!RegisterClassExW(&viewer_class)) return 1;

    const auto window = CreateWindowExW(0, klass.lpszClassName,
        L"RB4PC Builder | v" RB4PC_VERSION_W,
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 830, 590,
        nullptr, create_menu(), instance, nullptr);
    if (!window) return 1;
    ShowWindow(window, show_command);
    UpdateWindow(window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    app.logo.reset();
    if (app.gdiplus_token) Gdiplus::GdiplusShutdown(app.gdiplus_token);
    CoUninitialize();
    return static_cast<int>(message.wParam);
}

#endif
