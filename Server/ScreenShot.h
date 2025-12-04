#pragma once

#include <Windows.h>
#include <string>

// Chụp toàn bộ màn hình chính, trả về HBITMAP chứa ảnh screenshot.
// Caller phải gọi DeleteObject(hBitmap) sau khi dùng xong.
HBITMAP CaptureScreenToHBitmap();

// Lưu HBITMAP xuống file PNG (dùng GDI+ bên trong).
// filePath: đường dẫn file (unicode).
bool SaveHBitmapToPng(HBITMAP hBitmap, const std::wstring& filePath);

// Sinh tên file duy nhất dạng:
// baseName.ext, baseName (1).ext, baseName (2).ext, ...
// Ví dụ: "screen_capture", ".png" -> "screen_capture.png", "screen_capture (1).png", ...
std::wstring GetUniqueScreenshotFileName(
    const std::wstring& baseName = L"screen_capture",
    const std::wstring& ext = L".png"
);

// Hàm tiện lợi: chụp màn hình, lưu thành PNG với tên không trùng,
// nếu openAfterSave = true thì tự mở bằng viewer mặc định.
// outFilePath (nếu không null) sẽ nhận đường dẫn file đã lưu.
bool CaptureScreenAndSave(
    std::wstring* outFilePath = nullptr,
    const std::wstring& baseName = L"screen_capture",
    const std::wstring& ext = L".png",
    bool openAfterSave = false
);
