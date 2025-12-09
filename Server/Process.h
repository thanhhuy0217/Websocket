#pragma once
#include <string>
#include <vector>
#include <windows.h>
#include <iostream>

// Struct lưu thông tin ứng dụng từ Registry
struct AppInfo {
    std::string name; // Tên hiển thị (VD: Google Chrome)
    std::string path; // Đường dẫn exe chuẩn
};

class Process {
private:
    // --- CACHE DỮ LIỆU REGISTRY ---
    std::vector<AppInfo> m_installedApps;
    bool m_isListLoaded = false;

    // --- CÁC HÀM XỬ LÝ REGISTRY (Private) ---
    void LoadInstalledApps(); // Hàm nạp danh sách
    void ScanRegistryKey(HKEY hRoot, const char* subKey);
    std::string GetRegString(HKEY hKey, const char* valueName);
    std::string CleanPath(const std::string& rawPath); // Xử lý chuỗi "path,0" thành path sạch

    // --- CÁC HÀM TIỆN ÍCH ---
    std::string ToLower(std::string str);
    bool IsNumeric(const std::string& str);
    double GetProcessMemory(DWORD pid);

public:
    Process(); 
    bool StartProcess(const std::string& nameOrPath);
    std::string StopProcess(const std::string& nameOrPid);
    std::string ListProcesses();
};