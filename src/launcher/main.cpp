#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <string>
#include <vector>

#ifdef _MSC_VER
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#endif

namespace {

// Per-monitor V2 DPI awareness function signature
typedef BOOL(WINAPI* PFN_SetProcessDpiAwarenessContext)(DPI_AWARENESS_CONTEXT);

void enableHighDpi()
{
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
        auto pSetDpi = reinterpret_cast<PFN_SetProcessDpiAwarenessContext>(
            reinterpret_cast<void*>(GetProcAddress(user32, "SetProcessDpiAwarenessContext")));
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
        if (pSetDpi) {
            pSetDpi(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        }
    }
}

std::wstring getModuleDirectory()
{
    std::vector<wchar_t> buffer(MAX_PATH);
    DWORD len = 0;
    while (true) {
        len = GetModuleFileNameW(NULL, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (len == 0) {
            return std::wstring();
        }
        if (len < buffer.size()) {
            break;
        }
        buffer.resize(buffer.size() * 2);
    }

    std::wstring path(buffer.data(), len);
    size_t lastSlash = path.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        return path.substr(0, lastSlash);
    }
    return path;
}

std::wstring getDirectoryFromPath(const std::wstring& filePath)
{
    size_t lastSlash = filePath.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        return filePath.substr(0, lastSlash);
    }
    return std::wstring();
}

bool fileExists(const std::wstring& path)
{
    DWORD attr = GetFileAttributesW(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
}

std::wstring combinePaths(const std::wstring& dir, const std::wstring& relative)
{
    std::vector<wchar_t> out(MAX_PATH * 2);
    if (PathCombineW(out.data(), dir.c_str(), relative.c_str())) {
        return std::wstring(out.data());
    }
    return dir + L"\\" + relative;
}

std::wstring readIniTarget(const std::wstring& iniPath)
{
    if (!fileExists(iniPath)) {
        return std::wstring();
    }
    wchar_t target[MAX_PATH] = { 0 };
    DWORD read = GetPrivateProfileStringW(
        L"Launcher", L"Target", L"", target, static_cast<DWORD>(_countof(target)), iniPath.c_str());
    if (read > 0) {
        return std::wstring(target);
    }
    return std::wstring();
}

std::wstring getArgumentsFromCommandLine(const wchar_t* cmdLine)
{
    if (!cmdLine) {
        return std::wstring();
    }

    const wchar_t* p = cmdLine;
    while (*p == L' ' || *p == L'\t') {
        p++;
    }

    if (*p == L'"') {
        p++; // skip opening quote
        while (*p && *p != L'"') {
            p++;
        }
        if (*p == L'"') {
            p++; // skip closing quote
        }
    } else {
        while (*p && *p != L' ' && *p != L'\t') {
            p++;
        }
    }

    while (*p == L' ' || *p == L'\t') {
        p++;
    }

    return std::wstring(p);
}

void setupEnvironment(const std::wstring& targetDir)
{
    // Prioritize the target folder for DLL loading (Qt, FFmpeg, plugins)
    SetDllDirectoryW(targetDir.c_str());

    // Prepend targetDir to PATH for child processes
    DWORD pathLen = GetEnvironmentVariableW(L"PATH", NULL, 0);
    std::wstring newPath = targetDir;
    if (pathLen > 0) {
        std::vector<wchar_t> currentPath(pathLen);
        GetEnvironmentVariableW(L"PATH", currentPath.data(), pathLen);
        newPath += L";";
        newPath += currentPath.data();
    }
    SetEnvironmentVariableW(L"PATH", newPath.c_str());
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    (void)hInstance;
    (void)hPrevInstance;
    (void)pCmdLine;
    (void)nCmdShow;

    enableHighDpi();

    // Attach to parent console if invoked from Command Prompt or PowerShell
    BOOL hasConsole = AttachConsole(ATTACH_PARENT_PROCESS);

    std::wstring launcherDir = getModuleDirectory();
    std::wstring targetExe;
    std::vector<std::wstring> candidates;

    // 1. Check optional launcher.ini configuration
    std::wstring iniPath = combinePaths(launcherDir, L"launcher.ini");
    std::wstring iniTarget = readIniTarget(iniPath);
    if (!iniTarget.empty()) {
        std::wstring resolved = combinePaths(launcherDir, iniTarget);
        candidates.push_back(resolved);
        if (fileExists(resolved)) {
            targetExe = resolved;
        }
    }

    // 2. Search default candidate locations
    if (targetExe.empty()) {
        std::wstring cand1 = combinePaths(launcherDir, L"bin\\yave_app.exe"); // Launcher at root directory
        std::wstring cand2 = combinePaths(launcherDir, L"yave_app.exe");     // Launcher inside bin directory
        std::wstring cand3 = combinePaths(launcherDir, L"..\\bin\\yave_app.exe"); // Launcher in subfolder

        candidates.push_back(cand1);
        candidates.push_back(cand2);
        candidates.push_back(cand3);

        for (const auto& cand : candidates) {
            if (fileExists(cand)) {
                targetExe = cand;
                break;
            }
        }
    }

    if (targetExe.empty()) {
        std::wstring message =
            L"Unable to find the main application executable (yave_app.exe).\n\n"
            L"Searched paths:\n";
        for (const auto& cand : candidates) {
            message += L" - " + cand + L"\n";
        }
        message += L"\nPlease make sure the application is properly installed.";

        MessageBoxW(NULL, message.c_str(), L"YAVE Launcher Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    std::wstring targetDir = getDirectoryFromPath(targetExe);
    setupEnvironment(targetDir);

    // Build command line: "<targetExe>" <args>
    std::wstring childCmd = L"\"" + targetExe + L"\"";
    std::wstring args = getArgumentsFromCommandLine(GetCommandLineW());
    if (!args.empty()) {
        childCmd += L" ";
        childCmd += args;
    }

    // Prepare startup information
    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    DWORD creationFlags = 0;
    if (!hasConsole) {
        // Suppress creating a console window if launched without an active console
        creationFlags |= CREATE_NO_WINDOW;
    }

    // CreateProcessW requires a mutable buffer for the command line
    std::vector<wchar_t> cmdBuffer(childCmd.begin(), childCmd.end());
    cmdBuffer.push_back(L'\0');

    BOOL success = CreateProcessW(
        targetExe.c_str(),
        cmdBuffer.data(),
        NULL,
        NULL,
        hasConsole ? TRUE : FALSE,
        creationFlags,
        NULL,
        NULL, // Inherit current working directory
        &si,
        &pi);

    if (!success) {
        DWORD err = GetLastError();
        wchar_t errBuf[512] = { 0 };
        FormatMessageW(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            err,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            errBuf,
            static_cast<DWORD>(_countof(errBuf)),
            NULL);

        std::wstring errMsg =
            L"Failed to launch application:\n" + targetExe + L"\n\n"
            L"Error code " + std::to_wstring(err) + L": " + errBuf;

        MessageBoxW(NULL, errMsg.c_str(), L"YAVE Launcher Error", MB_ICONERROR | MB_OK);
        return static_cast<int>(err);
    }

    // Wait for target process to complete and forward exit code
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return static_cast<int>(exitCode);
}

// WinMain wrapper to support both ANSI and Unicode entry points across all toolchains (MinGW/MSVC)
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    (void)lpCmdLine;
    return wWinMain(hInstance, hPrevInstance, GetCommandLineW(), nCmdShow);
}
