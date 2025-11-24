
#include"ListApplication.h"

std::string Application::ToLowerStr(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

std::string Application::NormalizePath(const std::string& path) {
    std::string s = Trim(path);
    std::replace(s.begin(), s.end(), '/', '\\');
    return ToLowerStr(s);
}

// Lấy full path exe của process PID
bool Application::GetProcessImagePath(DWORD pid, std::string& outPath) {
    outPath.clear();

    HANDLE hProcess = OpenProcess(
        PROCESS_QUERY_LIMITED_INFORMATION,
        FALSE,
        pid
    );
    if (!hProcess) {
        return false;
    }

    char buffer[MAX_PATH];
    DWORD size = MAX_PATH;
    if (!QueryFullProcessImageNameA(hProcess, 0, buffer, &size)) {
        CloseHandle(hProcess);
        return false;
    }

    CloseHandle(hProcess);
    outPath.assign(buffer, size);
    return true;
}

// Kiểm tra còn process nào chạy bằng exePath này không
bool Application::HasProcessWithExe(const std::string& exePath) {
    std::string target = NormalizePath(exePath);

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(pe);

    if (!Process32First(hSnap, &pe)) {
        CloseHandle(hSnap);
        return false;
    }

    do {
        DWORD pid = pe.th32ProcessID;
        if (pid == 0) continue; // Idle

        std::string procPath;
        if (!GetProcessImagePath(pid, procPath)) continue;

        if (NormalizePath(procPath) == target) {
            CloseHandle(hSnap);
            return true;
        }

    } while (Process32Next(hSnap, &pe));

    CloseHandle(hSnap);
    return false;
}

// Kill tất cả process chạy bằng exePath
int Application::KillProcessesByExe(const std::string& exePath) {
    std::string target = NormalizePath(exePath);
    int killedCount = 0;

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(pe);

    if (!Process32First(hSnap, &pe)) {
        CloseHandle(hSnap);
        return 0;
    }

    do {
        DWORD pid = pe.th32ProcessID;
        if (pid == 0) continue;

        std::string procPath;
        if (!GetProcessImagePath(pid, procPath)) continue;

        if (NormalizePath(procPath) == target) {
            HANDLE hProc = OpenProcess(
                PROCESS_TERMINATE,
                FALSE,
                pid
            );
            if (hProc) {
                if (TerminateProcess(hProc, 0)) {
                    ++killedCount;
                }
                CloseHandle(hProc);
            }
        }

    } while (Process32Next(hSnap, &pe));

    CloseHandle(hSnap);
    return killedCount;
}

//================= STOP_APPLICATION =================
bool Application::StopApplicationByName(const std::string& displayName) {
    std::string targetName = ToLowerStr(Trim(displayName));
    if (targetName.empty()) return false;

    const ApplicationInfo* found = nullptr;

    for (const auto& app : g_applications) {
        if (ToLowerStr(Trim(app.name)) == targetName) {
            found = &app;
            break;
        }
    }

    if (!found || found->command.empty()) {
        return false;
    }

    if (!HasProcessWithExe(found->command)) {
        return true; // không có process nào => coi như đã stop rồi
    }

    KillProcessesByExe(found->command);
    return !HasProcessWithExe(found->command);
}

