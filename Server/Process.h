#pragma once
#include <string>
#include <vector>
#include <windows.h>
#include <tlhelp32.h>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <psapi.h>

// Định nghĩa chung Struct dùng cho cả 2 class
struct AppInfo {
    std::string name; // Tên hiển thị
    std::string path; // Đường dẫn exe chuẩn
};

class Process {
public:
    Process();

    // --- CÁC HÀM XỬ LÝ HỆ THỐNG ---

    // Đã đổi tên chuẩn thành StartProcess để khớp với main và Application
    bool StartProcess(const std::string& path);

    // Stop theo PID hoặc Tên Process
    std::string StopProcess(const std::string& nameOrPid);

    // Liệt kê process đang chạy (Snapshot)
    std::string ListProcesses();

    // --- CÁC HÀM TIỆN ÍCH (STATIC) ---
    static std::string ToLower(std::string str);
    static bool IsNumeric(const std::string& str);
    static std::string CleanPath(const std::string& rawPath);

    // Helper chuyển đổi Unicode sang String (Fix lỗi C2440)
    static std::string ConvertToString(const WCHAR* wstr);

protected:
    double GetProcessMemory(DWORD pid);
};