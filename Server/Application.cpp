#include "Application.h"

//----------------- helper xử lý chuỗi -----------------

// cắt khoảng trắng ở đầu và cuối chuỗi
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

// chuyển toàn bộ chuỗi thành chữ thường (lowercase)
std::string Application::ToLower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

// đọc một value dạng chuỗi (REG_SZ / REG_EXPAND_SZ) từ registry
bool Application::ExtractStringValueA(HKEY hKey, const char* valueName, std::string& out) {
    out.clear();

    DWORD type = 0;
    DWORD size = 0;
    LONG res = RegQueryValueExA(hKey, valueName, nullptr, &type, nullptr, &size);
    // nếu không phải kiểu chuỗi thì bỏ qua
    if (res != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) {
        return false;
    }

    if (size == 0) {
        out.clear();
        return true;
    }

    std::vector<char> buffer(size + 1);
    res = RegQueryValueExA(hKey, valueName, nullptr, nullptr,
        reinterpret_cast<LPBYTE>(buffer.data()), &size);
    if (res != ERROR_SUCCESS) {
        return false;
    }

    buffer[size] = '\0';
    out.assign(buffer.data());
    return true;
}

// từ DisplayIcon → tách ra đường dẫn exe
// ví dụ:
//   "\"C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe\",0"
//   "C:\\Program Files\\Zalo\\Zalo.exe,0"
std::string Application::ExtractExeFromDisplayIcon(const std::string& displayIcon) {
    std::string s = Trim(displayIcon);
    if (s.empty()) return "";

    // nếu bắt đầu bằng dấu " thì lấy phần nằm giữa 2 dấu "
    if (s[0] == '\"') {
        size_t secondQuote = s.find('\"', 1);
        if (secondQuote != std::string::npos && secondQuote > 1) {
            return s.substr(1, secondQuote - 1);
        }
    }

    // nếu có dấu phẩy: "path,0" → lấy phần trước dấu phẩy
    size_t commaPos = s.find(',');
    if (commaPos != std::string::npos) {
        s = s.substr(0, commaPos);
    }

    return Trim(s);
}

// kiểm tra path có phải file .exe thật sự tồn tại, không phải thư mục
bool Application::IsValidExePath(const std::string& path) {
    if (path.empty()) return false;

    std::string lower = ToLower(path);
    // kiểm tra đuôi .exe
    if (lower.size() < 4 || lower.compare(lower.size() - 4, 4, ".exe") != 0) {
        return false;
    }

    // kiểm tra tồn tại và không phải folder
    DWORD attr = GetFileAttributesA(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) return false;
    if (attr & FILE_ATTRIBUTE_DIRECTORY) return false;

    return true;
}

//----------------- duyệt registry Uninstall -----------------

// hàm này duyệt một nhánh Uninstall (ví dụ HKLM\...\Uninstall) để lấy danh sách app
void Application::EnumerateUninstallRoot(HKEY hRoot,
    const char* subkeyRoot,
    const char* /*rootPrefix*/)
{
    HKEY hUninstall = nullptr;
    LONG res = RegOpenKeyExA(hRoot, subkeyRoot, 0, KEY_READ, &hUninstall);
    if (res != ERROR_SUCCESS) {
        return; // không mở được nhánh → bỏ qua
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
            break; // duyệt hết
        }
        if (res != ERROR_SUCCESS) {
            ++index;
            continue; // lỗi lặt vặt, bỏ qua key này
        }

        HKEY hApp = nullptr;
        LONG resOpen = RegOpenKeyExA(hUninstall, subkeyName, 0, KEY_READ, &hApp);
        if (resOpen != ERROR_SUCCESS) {
            ++index;
            continue;
        }

        // 1. lấy DisplayName, nếu không có thì không coi là app
        std::string displayName;
        if (!ExtractStringValueA(hApp, "DisplayName", displayName) || displayName.empty()) {
            RegCloseKey(hApp);
            ++index;
            continue;
        }

        // 2. lấy DisplayIcon để suy ra đường dẫn exe
        std::string displayIcon;
        ExtractStringValueA(hApp, "DisplayIcon", displayIcon);

        std::string exePath;
        if (!displayIcon.empty()) {
            exePath = ExtractExeFromDisplayIcon(displayIcon);
        }

        // 3. chỉ nhận những app có exePath hợp lệ (tồn tại, là file .exe)
        if (!IsValidExePath(exePath)) {
            RegCloseKey(hApp);
            ++index;
            continue;
        }

        // 5. ghép thành ApplicationInfo (name + path) và cho vào vector
        ApplicationInfo info;
        info.name = displayName;
        info.path = exePath;

        g_applications.push_back(info);

        RegCloseKey(hApp);
        ++index;
    }

    RegCloseKey(hUninstall);
}

//----------------- kiểm tra process theo exePath -----------------

// chuẩn hoá path: trim, đổi / thành \, lowercase → để so sánh an toàn
std::string Application::NormalizePath(const std::string& path) {
    std::string s = Trim(path);
    std::replace(s.begin(), s.end(), '/', '\\');
    return ToLower(s);
}

// lấy full path exe của process từ pid
bool Application::GetProcessImagePath(DWORD pid, std::string& outPath) {
    outPath.clear();

    // mở process với quyền query thông tin
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

// duyệt toàn bộ process trong hệ thống,
// trả về true nếu tồn tại process có exePath trùng với exePath truyền vào
bool Application::HasProcessWithExe(const std::string& exePath) {
    std::string target = NormalizePath(exePath);

    // chụp snapshot tất cả process
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
        if (pid == 0) continue; // bỏ process idle

        std::string procPath;
        if (!GetProcessImagePath(pid, procPath)) continue;

        if (NormalizePath(procPath) == target) {
            // tìm thấy ít nhất một process trùng exePath
            CloseHandle(hSnap);
            return true;
        }

    } while (Process32Next(hSnap, &pe));

    CloseHandle(hSnap);
    return false;
}

//----------------- public api -----------------

// đọc registry, xây dựng lại danh sách ứng dụng đã cài
void Application::LoadInstalledApplications() {
    g_applications.clear();

    // app cài cho toàn máy (64-bit / chung)
    EnumerateUninstallRoot(
        HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
        "HKLM"
    );

    // app 32-bit trên hệ điều hành 64-bit
    EnumerateUninstallRoot(
        HKEY_LOCAL_MACHINE,
        "SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
        "HKLM32"
    );

    // app cài cho user hiện tại
    EnumerateUninstallRoot(
        HKEY_CURRENT_USER,
        "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
        "HKCU"
    );
}

// trả về vector<pair<ApplicationInfo, bool>>
// bool = true nếu hiện tại đang có process chạy exe tương ứng với app đó
std::vector<std::pair<ApplicationInfo, bool>> Application::ListApplicationsWithStatus() {
    std::vector<std::pair<ApplicationInfo, bool>> result;
    result.reserve(g_applications.size());

    for (const auto& app : g_applications) {
        bool running = false;
        if (!app.path.empty()) {
            // kiểm tra xem exe của app có process nào đang chạy không
            running = HasProcessWithExe(app.path);
        }
        result.emplace_back(app, running);
    }

    return result;
}


//===================================================START / STOP APPLICATION==================================

bool Application::StartApplication(const std::string& pathOrName) {
    // giao việc cho Process::StartProcess (đã hỗ trợ zoom, zoom.exe, notepad, full path...)
    return m_process.StartProcess(pathOrName);
}

bool Application::StopApplication(const std::string& nameOrPid) {
    std::string resultMsg = m_process.StopProcess(nameOrPid);

    // (tuỳ ý) in ra console để debug / xem log
    std::cout << "[StopApplication] " << resultMsg << "\n";

    // Nếu chuỗi bắt đầu bằng "Loi" thì coi là thất bại
    if (resultMsg.rfind("Loi", 0) == 0 || resultMsg.rfind("Loi:", 0) == 0) {
        return false;
    }

    // Còn lại coi như ok (đã kill được, hoặc vốn không có process nào)
    return true;
}
