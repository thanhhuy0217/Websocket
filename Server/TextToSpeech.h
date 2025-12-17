#pragma once
#include <string>
#include <thread>
#include <mutex>
#include <windows.h>
#include <sapi.h> // Thu vien Microsoft Speech API

// Link thu vien SAPI va OLE
#pragma comment(lib, "sapi.lib")
#pragma comment(lib, "ole32.lib")

class TextToSpeech {
private:
    // Ham ho tro chuyen doi UTF-8 (tu Web) sang Wide String (cho Windows)
    std::wstring Utf8ToWstring(const std::string& str);

    // Ham thuc thi viec noi (chay trong thread rieng)
    void SpeakLogic(std::string text);

public:
    TextToSpeech();
    ~TextToSpeech();

    // Ham goi tu ben ngoai
    void Speak(const std::string& text);
};