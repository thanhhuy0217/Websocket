#pragma once
#include "Process.h" // Kế thừa từ Process đã có đệ quy
#include <vector>
#include <unordered_set>

class Application : public Process {
private:
    // Helper check file đang chạy (Private nội bộ)
    std::unordered_set<std::string> BuildRunningExeSet();
    bool GetProcessImagePath(DWORD pid, std::string& outPath);

public:
    Application();

    // Hàm liệt kê danh sách app và trạng thái (Running/Stopped)
    std::vector<std::pair<AppInfo, bool>> ListApplicationsWithStatus();

    // In ra màn hình
    void PrintApplicationsWithStatus(size_t maxShow = 50);

    // Wrapper: Gọi lại hàm StartProcess (đã có đệ quy) của cha
    bool StartApplication(const std::string& pathOrName);

    // Wrapper: Gọi lại hàm StopProcess của cha
    bool StopApplication(const std::string& nameOrPid);
};