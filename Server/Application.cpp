#include "Application.h"
#include <iostream>

using namespace std;

Application::Application() {}

string Application::GetRegString(HKEY hKey, const char* valueName) {
    char buffer[1024];
    DWORD size = sizeof(buffer);
    DWORD type;
    if (RegQueryValueExA(hKey, valueName, NULL, &type, (LPBYTE)buffer, &size) == ERROR_SUCCESS) {
        if (type == REG_SZ || type == REG_EXPAND_SZ)
            return string(buffer);
    }
    return "";
}

void Application::EnumerateUninstallRoot(HKEY hRoot, const char* subkeyRoot) {
    HKEY hUninstall;
    if (RegOpenKeyExA(hRoot, subkeyRoot, 0, KEY_READ, &hUninstall) != ERROR_SUCCESS) return;

    char keyName[256];
    DWORD index = 0;
    DWORD keyNameLen = 256;

    while (RegEnumKeyExA(hUninstall, index, keyName, &keyNameLen, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
        HKEY hApp;
        if (RegOpenKeyExA(hUninstall, keyName, 0, KEY_READ, &hApp) == ERROR_SUCCESS) {
            string name = GetRegString(hApp, "DisplayName");
            string iconPath = GetRegString(hApp, "DisplayIcon");
            if (iconPath.empty()) iconPath = GetRegString(hApp, "InstallLocation");

            string exePath = Process::CleanPath(iconPath);

            if (!name.empty() && !exePath.empty() &&
                Process::ToLower(exePath).find(".exe") != string::npos) {
                m_installedApps.push_back({ name, exePath });
            }
            RegCloseKey(hApp);
        }
        keyNameLen = 256;
        index++;
    }
    RegCloseKey(hUninstall);
}

void Application::LoadInstalledApplications() {
    m_installedApps.clear();
    EnumerateUninstallRoot(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall");
    EnumerateUninstallRoot(HKEY_LOCAL_MACHINE, "SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall");
    EnumerateUninstallRoot(HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall");
}

unordered_set<string> Application::BuildRunningExeSet() {
    unordered_set<string> runningSet;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return runningSet;

    PROCESSENTRY32 pe32; pe32.dwSize = sizeof(pe32);
    if (Process32First(hSnap, &pe32)) {
        do {
            // SỬA LỖI UNICODE: Dùng hàm static của lớp cha
            string exeName = Process::ConvertToString(pe32.szExeFile);
            runningSet.insert(Process::ToLower(exeName));
        } while (Process32Next(hSnap, &pe32));
    }
    CloseHandle(hSnap);
    return runningSet;
}

vector<pair<AppInfo, bool>> Application::ListApplicationsWithStatus() {
    vector<pair<AppInfo, bool>> result;
    auto runningSet = BuildRunningExeSet();

    for (const auto& app : m_installedApps) {
        string exeName = app.path.substr(app.path.find_last_of("\\/") + 1);
        bool isRunning = runningSet.count(Process::ToLower(exeName));
        result.push_back({ app, isRunning });
    }
    return result;
}

void Application::PrintApplicationsWithStatus(size_t maxShow) {
    auto list = ListApplicationsWithStatus();
    size_t count = min(maxShow, list.size());
    for (size_t i = 0; i < count; ++i) {
        cout << "[" << i << "] " << list[i].first.name
            << " (" << (list[i].second ? "RUNNING" : "STOPPED") << ")\n";
    }
}

// --- START / STOP ---

bool Application::StartApplication(const string& nameOrPath) {
    string input = Process::ToLower(nameOrPath);

    if (m_installedApps.empty()) LoadInstalledApplications();

    for (const auto& app : m_installedApps) {
        if (Process::ToLower(app.name).find(input) != string::npos) {
            cout << "Found app: " << app.name << " -> Running: " << app.path << endl;
            // Gọi đúng tên hàm trong Process.h
            return Process::StartProcess(app.path);
        }
    }

    return Process::StartProcess(nameOrPath);
}

bool Application::StopApplication(const string& nameOrPid) {
    string res = Process::StopProcess(nameOrPid);
    cout << res << endl;
    return (res.find("Loi") == string::npos);
}