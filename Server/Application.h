#pragma once
#include <Windows.h>
#include <shellapi.h>

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <cctype>   // std::tolower
#include <algorithm>
#include <tlhelp32.h>   // CreateToolhelp32Snapshot, PROCESSENTRY32,...

struct ApplicationInfo {
    std::string id;              // ID nội bộ (prefix + subkey registry)
    std::string name;            // DisplayName: "Google Chrome", "Zalo", ...
    std::string command;         // Lệnh/đường dẫn để chạy (exe path)
    std::string uninstallString; // (optional) command gỡ cài đặt
};



class Application {
private:
    std::vector<ApplicationInfo> g_applications;
    std::map<std::string, size_t> g_appIndexById;
private:
    std::string Trim(const std::string& s);
    std::string ToLower(std::string s);
    bool ExtractStringValueA(HKEY hKey, const char* valueName, std::string& out);
    std::string ExtractExeFromDisplayIcon(const std::string& displayIcon);
    void EnumerateUninstallRoot(HKEY hRoot, const char* subkeyRoot, const char* rootPrefix);
    bool HasProcessWithExe(const std::string& exePath);
    bool GetProcessImagePath(DWORD pid, std::string& outPath);


    std::string NormalizePath(const std::string& path);
    std::string ToLowerStr(std::string s);

    bool StartApplicationByName(const std::string& displayName);
    bool StartProcessShell(const std::string& pathOrCommand);

    int KillProcessesByExe(const std::string& exePath);

public:
    std::vector<ApplicationInfo> LoadInstalledApplications();
    bool StopApplicationByName(const std::string& displayName);
    bool StartApplicationFromInput(const std::string& inputRaw);
    void printListApplication();

};
