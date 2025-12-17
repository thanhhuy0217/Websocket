#include "TextToSpeech.h"
#include <vector>

TextToSpeech::TextToSpeech() {
    // Constructor (co the khoi tao COM o day neu can, nhung tot nhat la trong thread dung no)
}

TextToSpeech::~TextToSpeech() {
}

// Chuyen doi chuoi tu Network (UTF-8) sang dinh dang Windows hieu (UTF-16/WideChar)
std::wstring TextToSpeech::Utf8ToWstring(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

// Logic chinh: Khoi tao Voice va doc
void TextToSpeech::SpeakLogic(std::string text) {
    // 1. Khoi tao thu vien COM tren thread nay
    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr)) return;

    // 2. Tao doi tuong Voice (ISpVoice)
    ISpVoice* pVoice = NULL;
    hr = CoCreateInstance(CLSID_SpVoice, NULL, CLSCTX_ALL, IID_ISpVoice, (void**)&pVoice);

    if (SUCCEEDED(hr)) {
        // 3. Chuyen text sang wstring
        std::wstring wText = Utf8ToWstring(text);

        // 4. Phat am thanh (SPF_DEFAULT: Dong bo, cho doc xong moi chay tiep lenh duoi)
        // Tuy nhien vi ham nay dang chay trong thread rieng, nen no khong lam treo Server chinh
        pVoice->Speak(wText.c_str(), SPF_DEFAULT, NULL);

        // 5. Giai phong bo nho
        pVoice->Release();
        pVoice = NULL;
    }

    // 6. Huy thu vien COM
    CoUninitialize();
}

void TextToSpeech::Speak(const std::string& text) {
    // Tao mot thread moi de doc ("Fire and Forget")
    // De server khong bi lag khi dang doc
    std::thread t(&TextToSpeech::SpeakLogic, this, text);
    t.detach(); // Chay ngam, khong can doi
}