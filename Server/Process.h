#pragma once
#include <string>
#include <vector>
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <shlobj.h>  // Cho Start Menu search
#include <shlwapi.h> // Cho thao tác path

// Định nghĩa struct dùng chung
struct AppInfo {
    std::string name; // Tên hiển thị
    std::string path; // Đường dẫn exe
};

class Process {
protected:
    // Dữ liệu dùng chung
    std::vector<AppInfo> m_installedApps;
    bool m_isListLoaded = false;

    // --- Các hàm nội bộ (Private Helpers) ---
    void ScanRegistryKey(HKEY hRoot, const char* subKey);
    std::string GetRegString(HKEY hKey, const char* valueName);
    double GetProcessMemory(DWORD pid);
    void ScanAppPaths(HKEY hRoot, const char* subKey);

    // Helper tìm kiếm file đệ quy (cho TH3)
    std::string FindFileRecursive(std::string directory, std::string fileToFind);
    // Helper quét Start Menu (cho TH3)
    std::string FindAppInStartMenu(std::string appName);

public:
    Process();

    // Load danh sách App
    void LoadInstalledApps();

    // Start App: Hệ thống -> Registry -> Start Menu
    bool StartProcess(const std::string& nameOrPath);

    // Stop & List
    std::string StopProcess(const std::string& nameOrPid);
    std::string ListProcesses();

    // Các hàm tiện ích (Static)
    static std::string ToLower(std::string str);
    static bool IsNumeric(const std::string& str);
    static std::string CleanPath(const std::string& rawPath);
    static std::string ConvertToString(const WCHAR* wstr);
};