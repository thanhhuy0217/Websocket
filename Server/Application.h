#pragma once
#define NOMINMAX
#include <Windows.h>
#include <shellapi.h>
#include <tlhelp32.h>

#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <cctype>

#include "Process.h"   // để dùng m_process.StartProcess / StopProcess

// struct mô tả 1 application (tương ứng với { name, exePath } bên Process.cpp)
struct ApplicationInfo {
    std::string name; // DisplayName trong Registry
    std::string path; // đường dẫn file .exe
};

class Application {
private:
    // danh sách ứng dụng đã cài (load từ Registry)
    std::vector<ApplicationInfo> g_applications;

    // dùng Process để Start / Stop
    Process m_process;

    // ===== helper nội bộ =====
    std::string Trim(const std::string& s);
    std::string ToLower(std::string s);
    bool ExtractStringValueA(HKEY hKey, const char* valueName, std::string& out);
    std::string ExtractExeFromDisplayIcon(const std::string& displayIcon);
    bool IsValidExePath(const std::string& path);

    void EnumerateUninstallRoot(HKEY hRoot,
        const char* subkeyRoot,
        const char* rootPrefix);

    std::string NormalizePath(const std::string& path);
    bool GetProcessImagePath(DWORD pid, std::string& outPath);
    bool HasProcessWithExe(const std::string& exePath);

public:
    // đọc registry, build lại g_applications
    void LoadInstalledApplications();

    // trả về vector<pair<ApplicationInfo, bool>>
    // bool = true nếu app đó đang có process chạy
    std::vector<std::pair<ApplicationInfo, bool>> ListApplicationsWithStatus();

    // start application (zoom, zoom.exe, notepad, full path, ...)
    bool StartApplication(const std::string& pathOrName);

    // stop application (tên exe hoặc PID)
    bool StopApplication(const std::string& nameOrPid);
};
