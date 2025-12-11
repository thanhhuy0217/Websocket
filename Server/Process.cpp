#include "Process.h"

using namespace std;

Process::Process() {}

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

// Hàm fix lỗi Unicode (C2440)
string Process::ConvertToString(const WCHAR* wstr) {
#ifdef UNICODE
    std::wstring ws(wstr);
    // Chuyển wstring sang string (ASCII)
    return std::string(ws.begin(), ws.end());
#else
    return std::string(wstr);
#endif
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

// --- SYSTEM ACTIONS ---

bool Process::StartProcess(const string& path) {
    // Đã đổi tên hàm cho khớp
    HINSTANCE res = ShellExecuteA(NULL, "open", path.c_str(), NULL, NULL, SW_SHOWNORMAL);
    return ((intptr_t)res > 32);
}

string Process::StopProcess(const string& nameOrPid) {
    if (IsNumeric(nameOrPid)) {
        DWORD pid = stoul(nameOrPid);
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (hProcess && TerminateProcess(hProcess, 1)) {
            CloseHandle(hProcess);
            return "Da diet PID " + nameOrPid;
        }
        return "Loi: Khong diet duoc PID " + nameOrPid;
    }

    // Kill by Name
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return "Loi Snapshot";

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    int count = 0;
    string target = ToLower(nameOrPid);

    if (Process32First(hSnap, &pe32)) {
        do {
            // SỬA LỖI C2440 TẠI ĐÂY: Dùng hàm ConvertToString
            string exeName = ConvertToString(pe32.szExeFile);
            string current = ToLower(exeName);

            if (current == target || current == target + ".exe") {
                HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
                if (hProc) { TerminateProcess(hProc, 1); CloseHandle(hProc); count++; }
            }
        } while (Process32Next(hSnap, &pe32));
    }
    CloseHandle(hSnap);
    return count > 0 ? "Da diet " + to_string(count) : "Khong tim thay";
}

string Process::ListProcesses() {
    stringstream ss;
    ss << "PID\tRAM(MB)\tName\n";

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return "Loi Snapshot";

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnap, &pe32)) {
        do {
            // SỬA LỖI C2440 TẠI ĐÂY NỮA
            string exeName = ConvertToString(pe32.szExeFile);
            double ram = GetProcessMemory(pe32.th32ProcessID);

            ss << pe32.th32ProcessID << "\t"
                << fixed << setprecision(1) << ram << "\t"
                << exeName << "\n";
        } while (Process32Next(hSnap, &pe32));
    }
    CloseHandle(hSnap);
    return ss.str();
}