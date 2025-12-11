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

// Định nghĩa struct dùng chung
struct AppInfo {
    std::string name; // Tên hiển thị
    std::string path; // Đường dẫn exe
};

class Process {
protected:
    // Dữ liệu dùng chung (Protected để Application kế thừa dùng được)
    std::vector<AppInfo> m_installedApps;
    bool m_isListLoaded = false;

    // Các hàm nội bộ
    void ScanRegistryKey(HKEY hRoot, const char* subKey);
    std::string GetRegString(HKEY hKey, const char* valueName);
    double GetProcessMemory(DWORD pid);

public:
    Process();

    // Load danh sách App (Application sẽ gọi hàm này)
    void LoadInstalledApps();

    // Start App: Tìm trong Registry trước, không thấy thì chạy lệnh
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