#define UNICODE
#define _UNICODE

#include <windows.h>
#include <shellapi.h>

#include <chrono>
#include <cwchar>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

static bool copyTree(const fs::path &source, const fs::path &target) {
    std::error_code ec;
    fs::create_directories(target, ec);
    if (ec) return false;

    for (fs::recursive_directory_iterator it(source, ec), end; it != end && !ec;
         it.increment(ec)) {
        const fs::path relative = fs::relative(it->path(), source, ec);
        if (ec || relative.empty()) return false;
        const fs::path destination = target / relative;
        if (it->is_symlink(ec)) return false;
        if (it->is_directory(ec)) {
            fs::create_directories(destination, ec);
        } else if (it->is_regular_file(ec)) {
            fs::create_directories(destination.parent_path(), ec);
            if (!ec)
                fs::copy_file(it->path(), destination,
                              fs::copy_options::overwrite_existing, ec);
        }
        if (ec) return false;
    }
    return !ec;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv || argc != 5) {
        if (argv) LocalFree(argv);
        return 2;
    }

    wchar_t *pidEnd = nullptr;
    const unsigned long parsedPid = std::wcstoul(argv[1], &pidEnd, 10);
    const bool pidIsValid = parsedPid != 0 && pidEnd && *pidEnd == L'\0';
    const fs::path source = fs::absolute(argv[2]).lexically_normal();
    const fs::path target = fs::absolute(argv[3]).lexically_normal();
    const std::wstring exeName = argv[4];
    LocalFree(argv);

    // Accept only the known EchoBox executable and ordinary, non-root folders.
    // All package bytes were SHA-256 verified by the main application before
    // this helper was started.
    if (!pidIsValid ||
        _wcsicmp(exeName.c_str(), L"EchoBoxII.exe") != 0 ||
        source == source.root_path() || target == target.root_path() ||
        source == target || !fs::is_directory(source) || !fs::is_directory(target) ||
        !fs::is_regular_file(source / exeName) ||
        !fs::is_regular_file(source / L"EchoBoxUpdater.exe") ||
        !fs::is_regular_file(target / exeName))
        return 3;

    if (HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, DWORD(parsedPid))) {
        WaitForSingleObject(process, 30000);
        CloseHandle(process);
    }

    // Keep a recoverable snapshot in TEMP. A failed backup must not damage the
    // current installation; copying the verified update starts only afterwards.
    const auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
    const fs::path backup = fs::temp_directory_path() /
        (L"EchoBoxII_backup_" + std::to_wstring(stamp));
    if (!copyTree(target, backup)) return 4;

    if (!copyTree(source, target)) {
        copyTree(backup, target);
        return 5;
    }

    const fs::path executable = target / exeName;
    SHELLEXECUTEINFOW launch{};
    launch.cbSize = sizeof(launch);
    launch.fMask = SEE_MASK_NOCLOSEPROCESS;
    launch.lpVerb = L"open";
    launch.lpFile = executable.c_str();
    launch.lpDirectory = target.c_str();
    launch.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&launch)) return 6;
    if (launch.hProcess) CloseHandle(launch.hProcess);
    return 0;
}
