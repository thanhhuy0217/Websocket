#pragma once
#include "Process.h"
#include <vector>
#include <unordered_set>

class Application : public Process {
private:
    std::vector<AppInfo> m_installedApps;

    void EnumerateUninstallRoot(HKEY hRoot, const char* subkeyRoot);
    std::string GetRegString(HKEY hKey, const char* valueName);
    std::unordered_set<std::string> BuildRunningExeSet();

public:
    Application();

    void LoadInstalledApplications();
    std::vector<std::pair<AppInfo, bool>> ListApplicationsWithStatus();
    void PrintApplicationsWithStatus(size_t maxShow = 50);

    // Tìm tên trong Registry rồi chạy
    bool StartApplication(const std::string& nameOrPath);

    // Dừng ứng dụng
    bool StopApplication(const std::string& nameOrPid);
};