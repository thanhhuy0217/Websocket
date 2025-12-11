#include "Process.h"

// Link thư viện hệ thống
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")

using namespace std;

// --- Helper Fix Lỗi Unicode ---
string Process::ConvertToString(const WCHAR* wstr) {
    if (!wstr) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
    if (size <= 0) return "";
    string str(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &str[0], size, NULL, NULL);
    return str;
}

Process::Process() { m_isListLoaded = false; }

// --- UTILS ---
string Process::ToLower(string str) {
    string res = str;
    transform(res.begin(), res.end(), res.begin(), ::tolower);
    return res;
}

bool Process::IsNumeric(const string& str) {
    if (str.empty()) return false;
    return all_of(str.begin(), str.end(), ::isdigit);
}

string Process::CleanPath(const string& rawPath) {
    string s = rawPath;
    s.erase(0, s.find_first_not_of(" \t\r\n"));
    s.erase(s.find_last_not_of(" \t\r\n") + 1);
    if (!s.empty() && s.front() == '\"') s.erase(0, 1);
    size_t quotePos = s.find('\"');
    if (quotePos != string::npos) s = s.substr(0, quotePos);
    size_t commaPos = s.find(',');
    if (commaPos != string::npos) s = s.substr(0, commaPos);
    return s;
}

string Process::GetRegString(HKEY hKey, const char* valueName) {
    char buffer[1024];
    DWORD size = sizeof(buffer);
    DWORD type;
    if (RegQueryValueExA(hKey, valueName, NULL, &type, (LPBYTE)buffer, &size) == ERROR_SUCCESS) {
        if (type == REG_SZ || type == REG_EXPAND_SZ) return string(buffer);
    }
    return "";
}

double Process::GetProcessMemory(DWORD pid) {
    PROCESS_MEMORY_COUNTERS pmc;
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess == NULL) return 0.0;
    if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
        CloseHandle(hProcess);
        return (double)pmc.WorkingSetSize / (1024 * 1024);
    }
    CloseHandle(hProcess);
    return 0.0;
}

void Process::ScanRegistryKey(HKEY hRoot, const char* subKey) {
    HKEY hUninstall;
    if (RegOpenKeyExA(hRoot, subKey, 0, KEY_READ, &hUninstall) != ERROR_SUCCESS) return;
    char keyName[256];
    DWORD index = 0;
    DWORD keyNameLen = 256;
    while (RegEnumKeyExA(hUninstall, index, keyName, &keyNameLen, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
        HKEY hApp;
        if (RegOpenKeyExA(hUninstall, keyName, 0, KEY_READ, &hApp) == ERROR_SUCCESS) {
            string name = GetRegString(hApp, "DisplayName");
            string iconPath = GetRegString(hApp, "DisplayIcon");
            if (iconPath.empty()) iconPath = GetRegString(hApp, "InstallLocation");
            if (!name.empty() && !iconPath.empty()) {
                string exePath = CleanPath(iconPath);
                if (ToLower(exePath).find(".exe") != string::npos) {
                    m_installedApps.push_back({ name, exePath });
                }
            }
            RegCloseKey(hApp);
        }
        keyNameLen = 256; index++;
    }
    RegCloseKey(hUninstall);
}

void Process::LoadInstalledApps() {
    m_installedApps.clear();
    ScanRegistryKey(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall");
    ScanRegistryKey(HKEY_LOCAL_MACHINE, "SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall");
    ScanRegistryKey(HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall");
    m_isListLoaded = true;
}

bool Process::StartProcess(const string& nameOrPath) {
    if (nameOrPath.empty()) return false;
    string input = ToLower(nameOrPath);
    if (input.find(":\\") != string::npos) {
        HINSTANCE res = ShellExecuteA(NULL, "open", nameOrPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
        return ((intptr_t)res > 32);
    }
    if (!m_isListLoaded) LoadInstalledApps();
    for (const auto& app : m_installedApps) {
        if (ToLower(app.name).find(input) != string::npos || ToLower(app.path).find(input) != string::npos) {
            HINSTANCE res = ShellExecuteA(NULL, "open", app.path.c_str(), NULL, NULL, SW_SHOWNORMAL);
            if ((intptr_t)res > 32) return true;
        }
    }
    HINSTANCE res = ShellExecuteA(NULL, "open", nameOrPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
    return ((intptr_t)res > 32);
}

string Process::StopProcess(const string& nameOrPid) {
    if (IsNumeric(nameOrPid)) {
        DWORD pid = stoul(nameOrPid);
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (hProcess && TerminateProcess(hProcess, 1)) { CloseHandle(hProcess); return "Killed PID " + nameOrPid; }
        return "Err: Cannot kill PID " + nameOrPid;
    }
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return "Err Snapshot";

    PROCESSENTRY32W pe32; pe32.dwSize = sizeof(PROCESSENTRY32W); // DUNG W
    int count = 0;
    string target = ToLower(nameOrPid);

    if (Process32FirstW(hSnap, &pe32)) {
        do {
            string exeName = ConvertToString(pe32.szExeFile);
            if (ToLower(exeName) == target || ToLower(exeName) == target + ".exe") {
                HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
                if (hProc) { TerminateProcess(hProc, 1); CloseHandle(hProc); count++; }
            }
        } while (Process32NextW(hSnap, &pe32));
    }
    CloseHandle(hSnap);
    return count > 0 ? "Killed " + to_string(count) : "Not found";
}

string Process::ListProcesses() {
    stringstream ss;
    ss << "PID\tRAM(MB)\tThreads\tName\n";
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return "Err Snapshot";
    PROCESSENTRY32W pe32; pe32.dwSize = sizeof(PROCESSENTRY32W); // DUNG W
    if (Process32FirstW(hSnap, &pe32)) {
        do {
            string exeName = ConvertToString(pe32.szExeFile);
            double ram = GetProcessMemory(pe32.th32ProcessID);
            ss << pe32.th32ProcessID << "\t" << fixed << setprecision(1) << ram << "\t" << pe32.cntThreads << "\t" << exeName << "\n";
        } while (Process32NextW(hSnap, &pe32));
    }
    CloseHandle(hSnap);
    return ss.str();
}