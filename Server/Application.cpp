#include "Application.h"
#include <iostream>

using namespace std;

Application::Application() {}

// --- STATUS CHECKER ---

bool Application::GetProcessImagePath(DWORD pid, string& outPath) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess) return false;
    char buffer[MAX_PATH];
    DWORD size = MAX_PATH;
    if (QueryFullProcessImageNameA(hProcess, 0, buffer, &size)) {
        outPath.assign(buffer, size);
        CloseHandle(hProcess);
        return true;
    }
    CloseHandle(hProcess);
    return false;
}

unordered_set<string> Application::BuildRunningExeSet() {
    unordered_set<string> runningSet;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return runningSet;

    PROCESSENTRY32W pe; // Dùng bản W để khớp với Process.cpp
    pe.dwSize = sizeof(pe);

    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (pe.th32ProcessID == 0) continue;
            string procPath;
            if (GetProcessImagePath(pe.th32ProcessID, procPath)) {
                runningSet.insert(Process::ToLower(Process::CleanPath(procPath)));
            }
            else {
                // Convert từ WCHAR -> String
                runningSet.insert(Process::ToLower(Process::ConvertToString(pe.szExeFile)));
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return runningSet;
}

// --- PUBLIC FUNCTIONS ---

vector<pair<AppInfo, bool>> Application::ListApplicationsWithStatus() {
    // 1. Nếu chưa load list, gọi hàm của lớp Cha
    if (!m_isListLoaded) {
        LoadInstalledApps();
    }

    vector<pair<AppInfo, bool>> result;
    auto runningSet = BuildRunningExeSet();

    // 2. Truy cập m_installedApps (của lớp Cha)
    for (const auto& app : m_installedApps) {
        string normalizedPath = Process::ToLower(Process::CleanPath(app.path));
        bool running = false;

        // Kiểm tra Running
        for (const auto& runPath : runningSet) {
            if (runPath.find(normalizedPath) != string::npos || normalizedPath.find(runPath) != string::npos) {
                running = true;
                break;
            }
        }
        result.push_back({ app, running });
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

// Gọi lại hàm Start của cha
bool Application::StartApplication(const string& pathOrName) {
    return Process::StartProcess(pathOrName);
}

// Gọi lại hàm Stop của cha
bool Application::StopApplication(const string& nameOrPid) {
    string res = Process::StopProcess(nameOrPid);
    return (res.find("Loi") == string::npos);
}