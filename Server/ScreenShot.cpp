#include "ScreenShot.h"

#include <Windows.h>
#include <vector>
#include <cstdint>

// link GDI + User32
#pragma comment(lib, "Gdi32.lib")
#pragma comment(lib, "User32.lib")

std::vector<std::uint8_t> ScreenShot()
{
    std::vector<std::uint8_t> buffer;

    // 1) Lấy kích thước màn hình
    int width = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);

    if (width <= 0 || height <= 0)
        return buffer;

    // 2) HDC nguồn (màn hình)
    HDC hScreenDC = GetDC(NULL);
    if (!hScreenDC)
        return buffer;

    // 3) HDC bộ nhớ
    HDC hMemDC = CreateCompatibleDC(hScreenDC);
    if (!hMemDC) {
        ReleaseDC(NULL, hScreenDC);
        return buffer;
    }

    // 4) Tạo bitmap tương thích
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
    if (!hBitmap) {
        DeleteDC(hMemDC);
        ReleaseDC(NULL, hScreenDC);
        return buffer;
    }

    // 5) Gán bitmap vào memory DC
    HBITMAP hOldBitmap = static_cast<HBITMAP>(SelectObject(hMemDC, hBitmap));

    // 6) Copy pixel từ màn hình vào memory DC
    BOOL ok = BitBlt(
        hMemDC,
        0, 0, width, height,
        hScreenDC,
        0, 0,
        SRCCOPY | CAPTUREBLT
    );

    // HDC màn hình không cần nữa
    ReleaseDC(NULL, hScreenDC);

    if (!ok) {
        SelectObject(hMemDC, hOldBitmap);
        DeleteObject(hBitmap);
        DeleteDC(hMemDC);
        buffer.clear();
        return buffer;
    }

    // 7) Chuẩn bị cấu trúc lấy DIB 32-bit BGRA
    BITMAPINFO bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = width;
    bi.bmiHeader.biHeight = -height; // âm -> top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;      // 32-bit BGRA
    bi.bmiHeader.biCompression = BI_RGB;

    // 8) Cấp buffer: width * height * 4 bytes
    buffer.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);

    // 9) Lấy pixel vào buffer
    int scanLines = GetDIBits(
        hMemDC,
        hBitmap,
        0,
        static_cast<UINT>(height),
        buffer.data(),
        &bi,
        DIB_RGB_COLORS
    );

    // 10) Dọn dẹp GDI
    SelectObject(hMemDC, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hMemDC);

    if (scanLines == 0) {
        buffer.clear(); // lỗi khi GetDIBits
    }

    return buffer;
}
