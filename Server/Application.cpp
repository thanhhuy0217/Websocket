#include"Application.h"

//================= Helper string =================

std::string Application::Trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
        ++start;
    }
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }
    return s.substr(start, end - start);
}

std::string Application::ToLower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

// Extract chuỗi REG_SZ/REG_EXPAND_SZ từ 1 value trong key
bool Application::ExtractStringValueA(HKEY hKey, const char* valueName, std::string& out) {
    DWORD type = 0;
    DWORD size = 0;
    LONG res = RegQueryValueExA(hKey, valueName, nullptr, &type, nullptr, &size);
    if (res != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) {
        return false;
    }

    if (size == 0) {
        out.clear();
        return true;
    }

    std::vector<char> buffer(size + 1);
    res = RegQueryValueExA(hKey, valueName, nullptr, nullptr, reinterpret_cast<LPBYTE>(buffer.data()), &size);
    if (res != ERROR_SUCCESS) {
        return false;
    }

    buffer[size] = '\0';
    out.assign(buffer.data());
    return true;
}

// Từ DisplayIcon → lấy đường dẫn exe
// VD: "C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe,0"
//     "\"C:\\Program Files\\Zalo\\Zalo.exe\",0"
std::string Application::ExtractExeFromDisplayIcon(const std::string& displayIcon) {
    std::string s = Trim(displayIcon);
    if (s.empty()) return "";

    // Nếu dạng "..." thì lấy phần trong ""
    if (s[0] == '\"') {
        size_t secondQuote = s.find('\"', 1);
        if (secondQuote != std::string::npos && secondQuote > 1) {
            return s.substr(1, secondQuote - 1);
        }
    }

    // Nếu có dấu phẩy: "path,0" -> lấy trước dấu phẩy
    size_t commaPos = s.find(',');
    if (commaPos != std::string::npos) {
        s = s.substr(0, commaPos);
    }

    return Trim(s);
}

//================= Liệt kê Uninstall keys =================

void Application::EnumerateUninstallRoot(HKEY hRoot, const char* subkeyRoot, const char* rootPrefix) {
    HKEY hUninstall = nullptr;
    LONG res = RegOpenKeyExA(hRoot, subkeyRoot, 0, KEY_READ, &hUninstall);
    if (res != ERROR_SUCCESS) {
        return;
    }

    char subkeyName[256];
    DWORD index = 0;

    while (true) {
        DWORD nameLen = sizeof(subkeyName);
        FILETIME ft{};
        res = RegEnumKeyExA(
            hUninstall,
            index,
            subkeyName,
            &nameLen,
            nullptr,
            nullptr,
            nullptr,
            &ft
        );

        if (res == ERROR_NO_MORE_ITEMS) {
            break;
        }
        if (res != ERROR_SUCCESS) {
            ++index;
            continue;
        }

        HKEY hApp = nullptr;
        LONG resOpen = RegOpenKeyExA(hUninstall, subkeyName, 0, KEY_READ, &hApp);
        if (resOpen != ERROR_SUCCESS) {
            ++index;
            continue;
        }

        // 1. Phải có DisplayName
        std::string displayName;
        if (!ExtractStringValueA(hApp, "DisplayName", displayName) || displayName.empty()) {
            RegCloseKey(hApp);
            ++index;
            continue;
        }

        // 2. Lấy DisplayIcon → suy ra command (exe path)
        std::string displayIcon;
        ExtractStringValueA(hApp, "DisplayIcon", displayIcon);

        std::string command;
        if (!displayIcon.empty()) {
            command = ExtractExeFromDisplayIcon(displayIcon);
        }

        // 3. Nếu command trống → KHÔNG coi là Application (theo định nghĩa mới)
        if (command.empty()) {
            RegCloseKey(hApp);
            ++index;
            continue;
        }

        // 4. Kiểm tra command có tồn tại & không phải folder
        DWORD attr = GetFileAttributesA(command.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY)) {
            RegCloseKey(hApp);
            ++index;
            continue;
        }

        // 5. (Optional) lấy thêm UninstallString
        std::string uninstallStr;
        ExtractStringValueA(hApp, "UninstallString", uninstallStr);

        ApplicationInfo app;
        app.id = std::string(rootPrefix) + "\\" + subkeyName;
        app.name = displayName;
        app.command = command;
        app.uninstallString = uninstallStr;

        g_appIndexById[app.id] = g_applications.size();
        g_applications.push_back(app);

        RegCloseKey(hApp);
        ++index;
    }

    RegCloseKey(hUninstall);
}

// Load tất cả application đã cài từ registry
std::vector<ApplicationInfo> Application::LoadInstalledApplications() {
    g_applications.clear();
    g_appIndexById.clear();

    // App 64-bit + app chung
    EnumerateUninstallRoot(
        HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
        "HKLM"
    );

    // App 32-bit trên OS 64-bit
    EnumerateUninstallRoot(
        HKEY_LOCAL_MACHINE,
        "SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
        "HKLM32"
    );

    // App cài cho user hiện tại
    EnumerateUninstallRoot(
        HKEY_CURRENT_USER,
        "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
        "HKCU"
    );
    return g_applications;
}

void Application::printListApplication() {
    size_t maxShow = std::min<size_t>(g_applications.size(), 50);
    for (size_t i = 0; i < maxShow; ++i) {
        const auto& app = g_applications[i];
        std::cout << "[" << i << "] " << app.name << "\n"
            << "     Id:  " << app.id << "\n"
            << "     Exe: " << app.command << "\n\n";
    }
    if (g_applications.size() > maxShow) {
        std::cout << "... (" << (g_applications.size() - maxShow)
            << " ung dung khac khong hien het)\n";
    }
}

//===================================================START APPLICATION==================================
bool Application::StartProcessShell(const std::string& pathOrCommand) {
    if (pathOrCommand.empty()) return false;

    HINSTANCE hInst = ShellExecuteA(
        nullptr,
        "open",
        pathOrCommand.c_str(),
        nullptr,
        nullptr,
        SW_SHOWNORMAL
    );

    INT_PTR code = reinterpret_cast<INT_PTR>(hInst);
    return code > 32;
}

bool Application::StartApplicationByName(const std::string& displayName) {
    std::string target = ToLower(displayName);

    for (const auto& app : g_applications) {
        if (ToLower(app.name) == target) {
            if (!app.command.empty()) {
                return StartProcessShell(app.command);
            }
            else {
                return StartProcessShell(app.name);
            }
        }
    }

    return false;
}

bool Application::StartApplicationFromInput(const std::string& inputRaw) {
    std::string input = Trim(inputRaw);
    if (input.empty()) return false;

    // 1) Thử coi input là DisplayName của app đã cài (ListApplication)
    std::string target = ToLowerStr(input);

    for (const auto& app : g_applications) {
        if (ToLowerStr(Trim(app.name)) == target) {
            // tìm thấy app theo DisplayName -> chạy theo exePath (command)
            if (!app.command.empty()) {
                return StartProcessShell(app.command);
            }
        }
    }

    // 2) Nếu không phải DisplayName, coi như user nhập exePath / tên lệnh
    //    giao trực tiếp cho ShellExecute (notepad, notepad.exe, C:\...\zalo.exe, ...).
    return StartProcessShell(input);
}


//======================================STOP APPLICATION=======================================================

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

