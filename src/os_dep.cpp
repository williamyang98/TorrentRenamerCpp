#include <string>
#include <optional>
#include <fmt/core.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <tchar.h>
#include <shobjidl.h>
#include <shtypes.h>

#pragma comment(lib, "mincore")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

// get rid of conflicting windows macros
#undef IGNORE
#undef DELETE

#include <stdexcept>
#include <shellapi.h>
#endif

#ifdef __linux__
#include <stdio.h>
#include <stdlib.h>
#endif


namespace os_dep {

#ifdef _WIN32
std::string wide_string_to_string(const std::wstring& wide_string)
{
    if (wide_string.empty())
    {
        return "";
    }

    const auto size_needed = WideCharToMultiByte(CP_UTF8, 0, &wide_string.at(0), (int)wide_string.size(), nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0)
    {
        throw std::runtime_error("WideCharToMultiByte() failed: " + std::to_string(size_needed));
    }

    std::string result(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wide_string.at(0), (int)wide_string.size(), &result.at(0), size_needed, nullptr, nullptr);
    return result;
}

std::optional<std::string> open_folder_dialog(void) {
    IFileOpenDialog *m_dialog;

    HRESULT hr = 
        CoCreateInstance(
            CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
            IID_IFileOpenDialog, reinterpret_cast<void**>(&m_dialog));

    if (!SUCCEEDED(hr)) {
        throw std::runtime_error("Failed to create file dialog object");
    }

    // setup the dialog
    m_dialog->SetOptions(FOS_PICKFOLDERS);

    // open the dialog
    hr = m_dialog->Show(NULL);
    if (!SUCCEEDED(hr)) return {};

    IShellItem *pItem;
    hr = m_dialog->GetResult(&pItem);
    if (!SUCCEEDED(hr)) return {};

    PWSTR pFilepath;
    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pFilepath);
    if (!SUCCEEDED(hr)) return {};

    std::wstring ws(pFilepath);
    return wide_string_to_string(ws);
}
#endif

#ifdef __linux__
std::optional<std::string> open_folder_dialog(void) {
    static constexpr int MAX_BUFFER_SIZE = 1024;
    static char buffer[MAX_BUFFER_SIZE];

    FILE *f = popen(
            "zenity --file-selection --directory "
            "--title=\"Select a folder\"", "r");

    char *res = fgets(buffer, MAX_BUFFER_SIZE-1, f);
    fclose(f);
    if (res == NULL) {
        return {};
    }
    const int n = strnlen(buffer, MAX_BUFFER_SIZE);
    // remove the newline character, which produces an invalid filepath
    buffer[n-1] = '\0';
    auto s = std::string(buffer, n-1);
    return s;
}
#endif

void open_folder(const std::string& path) {
    #ifdef _WIN32
    ShellExecuteA(NULL, "open", path.c_str(), NULL, NULL, SW_SHOW);
    #endif
    #ifdef __linux__
    const auto cmd = fmt::format("xdg-open \"{}\"", path.c_str());
    auto proc = system(cmd.c_str());
    #endif
}

void open_file(const std::string& path) {
    #ifdef _WIN32
    ShellExecuteA(NULL, "open", path.c_str(), NULL, NULL, SW_SHOW);
    #endif
    #ifdef __linux__
    const auto cmd = fmt::format("xdg-open \"{}\"", path.c_str());
    auto proc = system(cmd.c_str());
    #endif
}


};
