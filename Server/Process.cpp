#include <windows.h> 
#include <tlhelp32.h> 
#include <psapi.h>    
#include "Process.h"
#include <iomanip>    
#include <sstream>    
#include <algorithm>
#include <vector>

// Link thư viện tĩnh để không bị lỗi "unresolved external symbol" khi build
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "advapi32.lib")

using namespace std;

// dùng để chuyển đổi chuỗi WCHAR (Unicode) sang string thường (ASCII).
// Lý do: Project trong VS mặc định dùng Unicode nhưng code dùng std::string.
string WStringToString(const wstring& wstr) {
    return string(wstr.begin(), wstr.end());
}


Process::Process() {
    m_isListLoaded = false; // Ban đầu chưa load danh sách app
}

// Chuyển hết về chữ thường để so sánh cho dễ (VD: "Chrome" == "chrome")
string Process::ToLower(string str) {
    string res = str;
    transform(res.begin(), res.end(), res.begin(), ::tolower);
    return res;
}

// Kiểm tra xem chuỗi nhập vào có phải là số không (để phân biệt PID hay Tên)
bool Process::IsNumeric(const string& str) {
    if (str.empty()) return false;
    return all_of(str.begin(), str.end(), ::isdigit);
}

// Xử lý đường dẫn lấy từ Registry cho sạch đẹp
string Process::CleanPath(const string& rawPath) {
    string s = rawPath;
    if (!s.empty() && s.front() == '\"') s.erase(0, 1);
    if (!s.empty() && s.back() == '\"') s.pop_back();

    size_t commaPos = s.find(',');
    if (commaPos != string::npos) s = s.substr(0, commaPos);

    s.erase(remove(s.begin(), s.end(), '\"'), s.end());
    size_t first = s.find_first_not_of(' ');
    if (string::npos == first) return s;
    size_t last = s.find_last_not_of(' ');
    return s.substr(first, (last - first + 1));
}

// Đọc giá trị chuỗi từ key Registry an toàn
string Process::GetRegString(HKEY hKey, const char* valueName) {
    char buffer[1024];
    DWORD size = sizeof(buffer);
    DWORD type;
    if (RegQueryValueExA(hKey, valueName, NULL, &type, (LPBYTE)buffer, &size) == ERROR_SUCCESS) {
        if (type == REG_SZ || type == REG_EXPAND_SZ)
            return string(buffer);
    }
    return "";
}


// Quét một nhánh Registry (Uninstall) để tìm các phần mềm đã cài đặt
void Process::ScanRegistryKey(HKEY hRoot, const char* subKey) {
    HKEY hUninstall;
    if (RegOpenKeyExA(hRoot, subKey, 0, KEY_READ, &hUninstall) != ERROR_SUCCESS) return;

    char keyName[256];
    DWORD index = 0;
    DWORD keyNameLen = 256;

    // Lặp qua từng phần mềm trong danh sách
    while (RegEnumKeyExA(hUninstall, index, keyName, &keyNameLen, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
        HKEY hApp;
        if (RegOpenKeyExA(hUninstall, keyName, 0, KEY_READ, &hApp) == ERROR_SUCCESS) {
            string name = GetRegString(hApp, "DisplayName"); // Tên hiển thị

            // Ưu tiên lấy DisplayIcon vì nó trỏ thẳng vào file .exe
            string iconPath = GetRegString(hApp, "DisplayIcon");
            if (iconPath.empty()) iconPath = GetRegString(hApp, "InstallLocation");

            // Chỉ lưu vào danh sách nếu có đường dẫn exe hợp lệ
            if (!name.empty() && !iconPath.empty()) {
                string exePath = CleanPath(iconPath);
                if (ToLower(exePath).find(".exe") != string::npos) {
                    m_installedApps.push_back({ name, exePath });
                }
            }
            RegCloseKey(hApp);
        }
        keyNameLen = 256;
        index++;
    }
    RegCloseKey(hUninstall);
}

// Hàm gọi quét Registry ở 3 chỗ quan trọng (64bit, 32bit, User)
void Process::LoadInstalledApps() {
    m_installedApps.clear();
    ScanRegistryKey(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall");
    ScanRegistryKey(HKEY_LOCAL_MACHINE, "SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall");
    ScanRegistryKey(HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall");
    m_isListLoaded = true; // Đánh dấu là đã load xong
}


// Hàm Start
bool Process::StartProcess(const string& nameOrPath) {
    if (nameOrPath.empty()) return false;
    string input = ToLower(nameOrPath);


    string searchName = input;
    if (searchName.length() > 4 && searchName.substr(searchName.length() - 4) == ".exe") {
        searchName = searchName.substr(0, searchName.length() - 4);
    }

    if (!m_isListLoaded) LoadInstalledApps();

    // 1. Tìm trong danh sách app đã cài 
    for (const auto& app : m_installedApps) {
        string appNameLower = ToLower(app.name); // Tên hiển thị
        string appPathLower = ToLower(app.path); // Đường dẫn exe

        // ĐIỀU KIỆN 1: Tên hiển thị chứa từ khóa đã cắt đuôi?
        bool matchName = (appNameLower.find(searchName) != string::npos);

        // ĐIỀU KIỆN 2: Đường dẫn exe chứa input gốc?
        bool matchPath = (appPathLower.find(input) != string::npos);

        if (matchName || matchPath) {
            // Tìm thấy -> Chạy theo đường dẫn chuẩn trong Registry
            HINSTANCE res = ShellExecuteA(NULL, "open", app.path.c_str(), NULL, NULL, SW_SHOWNORMAL);
            if ((intptr_t)res > 32) return true;
        }
    }

    // 2. Fallback: Nếu không tìm thấy trong Registry, chạy lệnh hệ thống (notepad, calc...)
    HINSTANCE res = ShellExecuteA(NULL, "open", nameOrPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
    return ((intptr_t)res > 32);
}

// Hàm Stop
string Process::StopProcess(const string& nameOrPid) {
    // TH1: Nếu nhập số -> Diệt theo PID (Chính xác nhất)
    if (IsNumeric(nameOrPid)) {
        DWORD pid = stoul(nameOrPid);
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (hProcess && TerminateProcess(hProcess, 1)) {
            CloseHandle(hProcess);
            return "Da diet PID " + nameOrPid;
        }
        return "Loi: Khong diet duoc PID " + nameOrPid;
    }

    // TH2: Nếu nhập chữ -> Quét toàn bộ process đang chạy để tìm tên
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return "Loi Snapshot";

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    int count = 0;
    string target = ToLower(nameOrPid);

    // Duyệt danh sách process
    if (Process32First(hSnap, &pe32)) {
        do {
#ifdef UNICODE
            // Nếu máy đang chạy Unicode thì chuyển về string thường
            string exeName = WStringToString(pe32.szExeFile);
#else
            // Nếu không thì giữ nguyên
            string exeName = string(pe32.szExeFile);
#endif

            string current = ToLower(exeName);

            // So sánh tên (có hoặc không có đuôi .exe)
            if (current == target || current == target + ".exe") {
                HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
                if (hProc) {
                    if (TerminateProcess(hProc, 1)) count++;
                    CloseHandle(hProc);
                }
            }
        } while (Process32Next(hSnap, &pe32));
    }
    CloseHandle(hSnap);

    if (count > 0) return "Da diet " + to_string(count) + " process ten " + nameOrPid;
    else return "Khong tim thay process: " + nameOrPid;
}

// Hàm List: Liệt kê các process đang chạy
string Process::ListProcesses() {
    stringstream ss;
    ss << "PID\tRAM(MB)\tThreads\tName\n";

    // Chụp ảnh hệ thống (Snapshot)
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) return "Loi snapshot";

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hProcessSnap, &pe32)) {
        do {
            double ramUsage = GetProcessMemory(pe32.th32ProcessID);

            // Xử lý tên hiển thị để tránh lỗi font/ký tự lạ
#ifdef UNICODE
            string exeName = WStringToString(pe32.szExeFile);
#else
            string exeName = string(pe32.szExeFile);
#endif

            ss << pe32.th32ProcessID << "\t"
                << fixed << setprecision(1) << ramUsage << "\t"
                << pe32.cntThreads << "\t"
                << exeName << "\n";
        } while (Process32Next(hProcessSnap, &pe32));
    }
    CloseHandle(hProcessSnap);
    return ss.str();
}

// Lấy dung lượng RAM mà process đang dùng (tính bằng MB)
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