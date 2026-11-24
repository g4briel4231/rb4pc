#ifdef _WIN32

#define NOMINMAX
#include <windows.h>
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

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>
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
constexpr int id_asset_catalog = 1101;
constexpr int id_audio_catalog = 1102;
constexpr int id_character_catalog = 1103;
constexpr int id_texture_converter = 1104;
constexpr int id_corpus_audit = 1105;
constexpr int id_metadata_catalog = 1106;
constexpr int id_documentation = 1201;
constexpr int id_repository = 1202;
constexpr int id_about = 1203;

struct AppState {
    HWND convert = nullptr;
    HWND repair = nullptr;
    HWND open_archive = nullptr;
    HWND open_folder = nullptr;
    HWND status = nullptr;
    HFONT ui_font = nullptr;
    ULONG_PTR gdiplus_token = 0;
    std::unique_ptr<Gdiplus::Image> logo;
    std::filesystem::path executable_directory;
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
    set_status(L"Opened ForgeTool PC folder: " + path.wstring());
}

void generate_asset_catalog(HWND window) {
    const auto choice = MessageBoxW(window,
        L"Choose the PC source for the asset catalog.\n\n"
        L"Yes: select one *_pc.hdr file\nNo: select a folder recursively\nCancel: return",
        L"PC Asset Catalog", MB_YESNOCANCEL | MB_ICONQUESTION);
    if (choice == IDCANCEL) return;
    const auto source = choice == IDYES ? select_pc_header(window) : select_folder(window);
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
    const auto choice = MessageBoxW(window,
        L"Choose the PC source for the audio audit.\n\n"
        L"Yes: select one *_pc.hdr file\nNo: select a folder recursively\nCancel: return",
        L"PC Audio Audit", MB_YESNOCANCEL | MB_ICONQUESTION);
    if (choice == IDCANCEL) return;
    const auto source = choice == IDYES ? select_pc_header(window) : select_folder(window);
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
    const auto choice = MessageBoxW(window,
        L"Choose the PC source for the metadata catalog.\n\n"
        L"Yes: select one *_pc.hdr file\nNo: select a folder recursively\nCancel: return",
        L"PC Metadata Catalog", MB_YESNOCANCEL | MB_ICONQUESTION);
    if (choice == IDCANCEL) return;
    const auto source = choice == IDYES ? select_pc_header(window) : select_folder(window);
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
    const auto choice = MessageBoxW(window,
        L"Choose the PC source for the complete corpus audit.\n\n"
        L"Yes: select one *_pc.hdr file\nNo: select a folder recursively\nCancel: return",
        L"Complete PC Corpus Audit", MB_YESNOCANCEL | MB_ICONQUESTION);
    if (choice == IDCANCEL) return;
    const auto source = choice == IDYES ? select_pc_header(window) : select_folder(window);
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
    const auto choice = MessageBoxW(window,
        L"Choose the PC source for the character/animation catalog.\n\n"
        L"Yes: select one *_pc.hdr file\nNo: select a folder recursively\nCancel: return",
        L"Character/Animation Structural Profile", MB_YESNOCANCEL | MB_ICONQUESTION);
    if (choice == IDCANCEL) return;
    const auto source = choice == IDYES ? select_pc_header(window) : select_folder(window);
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

HMENU create_menu() {
    const auto menu = CreateMenu();
    const auto file = CreatePopupMenu();
    AppendMenuW(file, MF_STRING, id_open_archive, L"Open Package or Archive…");
    AppendMenuW(file, MF_STRING, id_open_folder, L"Open Folder…");
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

HWND create_button(HWND parent, int id, const wchar_t* text) {
    const auto button = CreateWindowExW(0, L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        0, 0, 100, 40, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr), nullptr);
    SendMessageW(button, WM_SETFONT, reinterpret_cast<WPARAM>(app.ui_font), TRUE);
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

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_CREATE: {
            app.ui_font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            app.convert = create_button(window, id_convert, L"Convert Rock Band 4 PS4 to PC");
            app.repair = create_button(window, id_repair, L"Repair Rock Band 4 PC");
            app.open_archive = create_button(window, id_open_archive, L"Open a ForgeTool PC File…");
            app.open_folder = create_button(window, id_open_folder, L"Open a Folder…");
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
        case WM_PAINT: paint_logo(window); return 0;
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
                case id_asset_catalog: generate_asset_catalog(window); return 0;
                case id_audio_catalog: generate_audio_catalog(window); return 0;
                case id_character_catalog: generate_character_catalog(window); return 0;
                case id_corpus_audit: generate_corpus_audit(window); return 0;
                case id_metadata_catalog: generate_metadata_catalog(window); return 0;
                case id_texture_converter: show_tool(window, L"Texture Converter", L"rb4pc-texture-probe --convert-pc"); return 0;
                case id_documentation: {
                    const auto docs = find_asset(L"docs");
                    open_path_or_url(docs.empty() ? L"https://github.com/g4briel4231/rb4pc/tree/main/docs" : docs.wstring());
                    return 0;
                }
                case id_repository: open_path_or_url(L"https://github.com/g4briel4231/rb4pc"); return 0;
                case id_about:
                    MessageBoxW(window,
                        L"RB4PC Builder v" RB4PC_VERSION_W L"\n\nIndependent clean-room tooling for user-supplied Rock Band 4 assets.\nNo proprietary game content is included.",
                        L"About RB4PC Builder", MB_OK | MB_ICONINFORMATION);
                    return 0;
                case id_exit: DestroyWindow(window); return 0;
            }
            break;
        case WM_DESTROY:
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
