#pragma once
#include "Process.h" // Kế thừa từ Process
#include <vector>
#include <unordered_set>

class Application : public Process {
private:
    // Helper check file đang chạy
    std::unordered_set<std::string> BuildRunningExeSet();
    bool GetProcessImagePath(DWORD pid, std::string& outPath);

public:
    Application();

    // Hàm này dùng lại dữ liệu m_installedApps của Process
    std::vector<std::pair<AppInfo, bool>> ListApplicationsWithStatus();
    void PrintApplicationsWithStatus(size_t maxShow = 50);

    // Wrapper để đồng bộ tên hàm gọi từ Main
    bool StartApplication(const std::string& pathOrName);
    bool StopApplication(const std::string& nameOrPid);
};