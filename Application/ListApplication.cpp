#include"ListApplication.h"

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















//#include"Application.h"
//
//
//bool Application::isToolWindow(HWND hWnd) {
//	LONG_PTR styleEx = GetWindowLongPtr(hWnd, GWL_EXSTYLE); //Lấy ra các cờ mở rộng của từng HWND để xem là nó có phải toolWindow hông?
//	if (styleEx & WS_EX_TOOLWINDOW) return true;
//	else return false;
//}
//
//bool Application::isAltTabWindow(HWND hWnd) {
//	if (GetWindow(hWnd, GW_OWNER) != NULL) {
//		return false;
//	}
//
//	if (isToolWindow(hWnd)) return false;
//
//	if (!IsWindowVisible(hWnd)) return false;
//
//	char title[256];
//	int len = GetWindowTextA(hWnd, title, sizeof(title));
//	if (len == 0) return false;
//
//	return true;
//}
//std::string Application::ConvertWideToUtf8(const wchar_t* wstr) {
//	if (!wstr) return std::string();
//
//	// Dùng -1 để Windows tự tính độ dài dựa trên '\0'
//	int sizeNeeded = WideCharToMultiByte(
//		CP_UTF8,
//		0,
//		wstr,
//		-1,         // null-terminated
//		nullptr,
//		0,
//		nullptr,
//		nullptr
//	);
//
//	if (sizeNeeded <= 0) {
//		return std::string();
//	}
//
//	std::string result(sizeNeeded - 1, 0); // bỏ ký tự '\0' cuối
//	WideCharToMultiByte(
//		CP_UTF8,
//		0,
//		wstr,
//		-1,
//		&result[0],
//		sizeNeeded,
//		nullptr,
//		nullptr
//	);
//
//	return result;
//}
//
//
//std::string Application::QueryProcessImagePath(DWORD pid) {
//	HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
//	if (hProcess == NULL) return"";
//	wchar_t buffer[MAX_PATH];
//	unsigned long size = MAX_PATH;
//	bool ok = QueryFullProcessImageName(hProcess, 0, buffer, &size);
//
//	CloseHandle(hProcess);
//
//	if (!ok) {
//		return "";
//	}
//	else return ConvertWideToUtf8(buffer);
//}
//
//std::string Application::ExtractFileName(const std::string& exePath) {
//	// Tìm vị trí của '\' hoặc '/' cuối cùng (cho chắc cross-platform)
//	std::size_t pos = exePath.find_last_of("\\");
//
//	if (pos == std::string::npos) {
//		// Không có '\' hoặc '/', trả luôn cả chuỗi
//		return exePath;
//	}
//
//	return exePath.substr(pos + 1);
//}
//
//BOOL CALLBACK Application::EnumWindowCallback(HWND hWnd, LPARAM lParam)
//{
//	auto* self = reinterpret_cast<Application*>(lParam);
//
//	// Nếu không phải app kiểu Alt+Tab thì bỏ qua, nhưng vẫn tiếp tục duyệt
//	if (!self->isAltTabWindow(hWnd))
//		return TRUE;
//
//	DWORD pid = 0;
//	GetWindowThreadProcessId(hWnd, &pid);
//
//	// Tìm xem PID này đã có AppInfor chưa
//	auto it = self->m_pidToApp.find(pid);
//
//	if (it == self->m_pidToApp.end()) {
//		// Lần đầu thấy PID này → tạo AppInfor mới
//
//		std::string exePath = self->QueryProcessImagePath(pid);
//		std::string exeName = self->ExtractFileName(exePath);
//
//		char titleBuf[256] = { 0 };
//		int len = GetWindowTextA(hWnd, titleBuf, sizeof(titleBuf));
//		std::string mainTitle;
//		if (len > 0)
//			mainTitle.assign(titleBuf, len);
//
//		// Tạo object ngay trong list (list quản lý lifetime)
//		self->m_ListApp.emplace_back(pid, exePath, exeName, mainTitle, hWnd);
//
//		// Lấy pointer tới phần tử vừa thêm
//		AppInfor* app = &self->m_ListApp.back();
//		self->m_pidToApp[pid] = app;
//	}
//	else {
//		// Đã có AppInfor cho PID này → chỉ thêm hWnd vào list window
//		AppInfor* app = it->second;
//		app->_windows.push_back(hWnd);
//	}
//
//	return TRUE; // tiếp tục duyệt các cửa sổ khác
//}
//
//
//std::list<AppInfor> Application::ListApplication() {
//	m_ListApp.clear();
//	m_pidToApp.clear();
//
//	//EnumWindows(EnumWindowCallback, 0);
//	EnumWindows(&Application::EnumWindowCallback,
//		reinterpret_cast<LPARAM>(this));
//
//	return m_ListApp;
//}
