#include "Process.h"

// Link thư viện hệ thống
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib") // Quan trọng cho ShellExecute

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
    // Xóa khoảng trắng đầu cuối
    size_t first = s.find_first_not_of(" \t\r\n");
    if (string::npos == first) return "";
    size_t last = s.find_last_not_of(" \t\r\n");
    s = s.substr(first, (last - first + 1));

    // Xóa dấu ngoặc kép nếu có
    s.erase(remove(s.begin(), s.end(), '\"'), s.end());

    // Cắt các tham số sau dấu phẩy (nếu có trong Registry)
    size_t commaPos = s.find(',');
    if (commaPos != string::npos) s = s.substr(0, commaPos);

    return s;
}

// Thay thế hàm GetRegString cũ trong Process.cpp bằng hàm này :
string Process::GetRegString(HKEY hKey, const char* valueName) {
    // 1. Chuyển tên Value (ví dụ "DisplayName") từ char* sang WCHAR* để dùng API Unicode
    int len = MultiByteToWideChar(CP_ACP, 0, valueName, -1, NULL, 0);
    if (len <= 0) return "";
    wstring wValueName(len, 0);
    MultiByteToWideChar(CP_ACP, 0, valueName, -1, &wValueName[0], len);

    // 2. Dùng RegQueryValueExW (Phiên bản Unicode) thay vì A
    WCHAR buffer[4096]; // Tăng buffer lên 4096 để tránh bị tràn với các app tên dài
    DWORD size = sizeof(buffer);
    DWORD type;

    // Lưu ý: wValueName.c_str() cắt bỏ ký tự null thừa ở cuối nếu có
    if (RegQueryValueExW(hKey, wValueName.data(), NULL, &type, (LPBYTE)buffer, &size) == ERROR_SUCCESS) {
        // Chỉ lấy dữ liệu dạng chuỗi
        if (type == REG_SZ || type == REG_EXPAND_SZ) {
            // 3. Quan trọng: Chuyển từ WCHAR (Unicode) sang UTF-8 bằng hàm có sẵn của bạn
            return ConvertToString(buffer);
        }
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

// --- REGISTRY SCANNING ---
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
                // Chỉ lấy nếu là exe
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

void Process::ScanAppPaths(HKEY hRoot, const char* subKey) {
    HKEY hAppPaths;
    if (RegOpenKeyExA(hRoot, subKey, 0, KEY_READ, &hAppPaths) != ERROR_SUCCESS)
        return;

    char keyName[256];
    DWORD index = 0;

    while (true) {
        DWORD nameLen = sizeof(keyName);
        FILETIME ft{};
        LONG res = RegEnumKeyExA(
            hAppPaths,
            index,
            keyName,
            &nameLen,
            nullptr,
            nullptr,
            nullptr,
            &ft
        );

        if (res == ERROR_NO_MORE_ITEMS) {
            break; // duyệt hết
        }
        if (res != ERROR_SUCCESS) {
            ++index;
            continue; // lỗi lặt vặt → skip key này
        }

        // Mở subkey: ví dụ "notepad.exe"
        HKEY hEntry;
        if (RegOpenKeyExA(hAppPaths, keyName, 0, KEY_READ, &hEntry) != ERROR_SUCCESS) {
            ++index;
            continue;
        }

        // Đọc default value (tên value = NULL)
        char  buffer[1024];
        DWORD size = sizeof(buffer);
        DWORD type = 0;
        std::string exePath;

        if (RegQueryValueExA(hEntry, nullptr, nullptr, &type,
            (LPBYTE)buffer, &size) == ERROR_SUCCESS) {
            if (type == REG_SZ || type == REG_EXPAND_SZ) {
                // buffer đã là chuỗi C kết thúc \0
                exePath.assign(buffer);
            }
        }

        RegCloseKey(hEntry);

        if (exePath.empty()) {
            ++index;
            continue;
        }

        // Làm sạch path và check .exe
        exePath = CleanPath(exePath);
        std::string lowerPath = ToLower(exePath);
        if (lowerPath.find(".exe") == std::string::npos) {
            ++index;
            continue;
        }

        // Lấy tên hiển thị từ tên key, VD "notepad.exe" -> "notepad"
        std::string displayName = keyName;
        std::string lowerKey = ToLower(displayName);
        if (lowerKey.size() > 4 &&
            lowerKey.substr(lowerKey.size() - 4) == ".exe") {
            displayName = displayName.substr(0, displayName.size() - 4);
        }

        // Tránh trùng app đã có từ Uninstall: so sánh theo path normalize
        bool exists = false;
        std::string normalizedNew = ToLower(CleanPath(exePath));
        for (const auto& app : m_installedApps) {
            std::string normalizedOld = ToLower(CleanPath(app.path));
            if (normalizedOld == normalizedNew) {
                exists = true;
                break;
            }
        }

        if (!exists) {
            // AppInfo { name, path }
            m_installedApps.push_back({ displayName, exePath });
        }

        ++index;
    }

    RegCloseKey(hAppPaths);
}


void Process::LoadInstalledApps() {
    m_installedApps.clear();

    // 1. App cài đặt tiêu chuẩn (có Uninstall)
    ScanRegistryKey(HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall");
    ScanRegistryKey(HKEY_LOCAL_MACHINE,
        "SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall");
    ScanRegistryKey(HKEY_CURRENT_USER,
        "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall");

    // 2. App có đăng ký App Paths (rất nhiều app hệ thống + user app)
    ScanAppPaths(HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths");
    ScanAppPaths(HKEY_CURRENT_USER,
        "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths");

    m_isListLoaded = true;
}



// --- RECURSIVE SEARCH HELPERS ---

string Process::FindFileRecursive(string directory, string fileToFind) {
    string searchPath = directory + "\\*";
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE) return "";

    string resultPath = "";
    string lowerFind = ToLower(fileToFind);

    do {
        string currentName = findData.cFileName;
        if (currentName == "." || currentName == "..") continue;

        string fullPath = directory + "\\" + currentName;

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            // Đệ quy vào thư mục con
            string found = FindFileRecursive(fullPath, fileToFind);
            if (!found.empty()) {
                resultPath = found;
                break;
            }
        }
        else {
            string lowerName = ToLower(currentName);
            // Logic tìm kiếm: Tên file chứa từ khóa VÀ phải là .exe hoặc .lnk (shortcut)
            if (lowerName.find(lowerFind) != string::npos) {
                if (lowerName.find(".lnk") != string::npos || lowerName.find(".exe") != string::npos) {
                    resultPath = fullPath;
                    break;
                }
            }
        }
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);
    return resultPath;
}

string Process::FindAppInStartMenu(string appName) {
    char path[MAX_PATH];
    string foundPath = "";

    // Các folder Start Menu phổ biến
    vector<int> foldersToCheck = {
        CSIDL_COMMON_PROGRAMS, // Start Menu (All Users)
        CSIDL_PROGRAMS,        // Start Menu (Current User)
        CSIDL_DESKTOPDIRECTORY // Desktop
    };

    for (int folderId : foldersToCheck) {
        if (SHGetFolderPathA(NULL, folderId, NULL, 0, path) == S_OK) {
            foundPath = FindFileRecursive(string(path), appName);
            if (!foundPath.empty()) return foundPath;
        }
    }
    return "";
}

// --- START PROCESS (CORE LOGIC) ---
bool Process::StartProcess(const string& nameOrPath) {
    if (nameOrPath.empty()) return false;
    string input = ToLower(nameOrPath);

    // Xử lý cắt đuôi .exe nếu người dùng nhập (ví dụ "discord.exe" -> "discord")
    string searchName = input;
    if (searchName.length() > 4 && searchName.substr(searchName.length() - 4) == ".exe") {
        searchName = searchName.substr(0, searchName.length() - 4);
    }

    // --- TH1: Thử chạy trực tiếp (Hệ thống hoặc đường dẫn đầy đủ) ---
    // Ví dụ: "calc", "notepad", "C:\\Windows\\System32\\cmd.exe"
    // Nếu > 32 là thành công
    HINSTANCE res = ShellExecuteA(NULL, "open", nameOrPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
    if ((intptr_t)res > 32) return true;

    // --- TH2: Tìm trong danh sách Registry đã load ---
    if (!m_isListLoaded) LoadInstalledApps();

    for (const auto& app : m_installedApps) {
        // So sánh tên hiển thị hoặc đường dẫn
        if (ToLower(app.name).find(searchName) != string::npos || ToLower(app.path).find(searchName) != string::npos) {
            HINSTANCE res = ShellExecuteA(NULL, "open", app.path.c_str(), NULL, NULL, SW_SHOWNORMAL);
            if ((intptr_t)res > 32) return true;
        }
    }

    // --- TH3: Tìm đệ quy trong Start Menu (Fallback cuối cùng) ---
    // Discord và các app cài User thường nằm ở đây dưới dạng shortcut
    string shortcutPath = FindAppInStartMenu(searchName);
    if (!shortcutPath.empty()) {
        // Chạy file .lnk hoặc .exe tìm được
        HINSTANCE res = ShellExecuteA(NULL, "open", shortcutPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
        return ((intptr_t)res > 32);
    }

    return false;
}

// --- STOP & LIST PROCESS ---
string Process::StopProcess(const string& nameOrPid) {
    if (IsNumeric(nameOrPid)) {
        DWORD pid = stoul(nameOrPid);
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (hProcess && TerminateProcess(hProcess, 1)) {
            CloseHandle(hProcess);
            return "Killed PID " + nameOrPid;
        }
        return "Err: Cannot kill PID " + nameOrPid;
    }

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return "Err Snapshot";

    PROCESSENTRY32W pe32; pe32.dwSize = sizeof(PROCESSENTRY32W);
    int count = 0;
    string target = ToLower(nameOrPid);

    if (Process32FirstW(hSnap, &pe32)) {
        do {
            string exeName = ConvertToString(pe32.szExeFile);
            string lowerName = ToLower(exeName);
            if (lowerName == target || lowerName == target + ".exe") {
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

    PROCESSENTRY32W pe32; pe32.dwSize = sizeof(PROCESSENTRY32W);
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