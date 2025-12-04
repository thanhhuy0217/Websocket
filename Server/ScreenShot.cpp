#include "ScreenShot.h"

#include <gdiplus.h>
#include <shellapi.h>
#include <vector>
#include <sstream>

#pragma comment(lib, "Gdiplus.lib")
#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Gdi32.lib")
#pragma comment(lib, "Shell32.lib")

// ================== Helper ẩn trong file (anonymous namespace) ==================
namespace {

    // Lấy CLSID encoder (ở đây dùng cho PNG)
    int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
        UINT num = 0;
        UINT size = 0;

        Gdiplus::GetImageEncodersSize(&num, &size);
        if (size == 0) {
            return -1; // không có encoder
        }

        std::vector<BYTE> buffer(size);
        Gdiplus::ImageCodecInfo* pImageCodecInfo =
            reinterpret_cast<Gdiplus::ImageCodecInfo*>(buffer.data());

        Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);

        for (UINT i = 0; i < num; ++i) {
            if (wcscmp(pImageCodecInfo[i].MimeType, format) == 0) {
                *pClsid = pImageCodecInfo[i].Clsid;
                return static_cast<int>(i);
            }
        }

        return -1;
    }

    // Kiểm tra file có tồn tại không
    bool FileExists(const std::wstring& path) {
        DWORD attr = GetFileAttributesW(path.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES) {
            return false;
        }
        // Nếu là thư mục thì coi như không phải file ảnh hợp lệ
        if (attr & FILE_ATTRIBUTE_DIRECTORY) {
            return false;
        }
        return true;
    }

} 

HBITMAP CaptureScreenToHBitmap() {
    // Lấy kích thước màn hình chính
    int width = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);

    // HDC nguồn (màn hình)
    HDC hScreenDC = GetDC(NULL);
    if (!hScreenDC) {
        return NULL;
    }

    // HDC bộ nhớ
    HDC hMemDC = CreateCompatibleDC(hScreenDC);
    if (!hMemDC) {
        ReleaseDC(NULL, hScreenDC);
        return NULL;
    }

    // Tạo bitmap tương thích
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
    if (!hBitmap) {
        DeleteDC(hMemDC);
        ReleaseDC(NULL, hScreenDC);
        return NULL;
    }

    // Gán bitmap vào memory DC
    HBITMAP hOldBitmap =
        static_cast<HBITMAP>(SelectObject(hMemDC, hBitmap));

    // Copy pixel từ màn hình vào memory DC
    BOOL ok = BitBlt(
        hMemDC,
        0, 0, width, height,
        hScreenDC,
        0, 0,
        SRCCOPY | CAPTUREBLT
    );

    // Khôi phục object cũ
    SelectObject(hMemDC, hOldBitmap);

    // Giải phóng HDC
    DeleteDC(hMemDC);
    ReleaseDC(NULL, hScreenDC);

    if (!ok) {
        DeleteObject(hBitmap);
        return NULL;
    }

    // hBitmap lúc này chứa screenshot
    return hBitmap;
}

bool SaveHBitmapToPng(HBITMAP hBitmap, const std::wstring& filePath) {
    if (!hBitmap) {
        return false;
    }

    // Khởi tạo GDI+
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken = 0;
    if (Gdiplus::GdiplusStartup(&gdiplusToken,
        &gdiplusStartupInput,
        NULL) != Gdiplus::Ok) {
        return false;
    }

    bool result = false;

    {
        Gdiplus::Bitmap bitmap(hBitmap, NULL);

        CLSID pngClsid;
        if (GetEncoderClsid(L"image/png", &pngClsid) >= 0) {
            Gdiplus::Status status =
                bitmap.Save(filePath.c_str(), &pngClsid, NULL);
            result = (status == Gdiplus::Ok);
        }
    }

    // Shutdown GDI+
    Gdiplus::GdiplusShutdown(gdiplusToken);

    return result;
}

std::wstring GetUniqueScreenshotFileName(const std::wstring& baseName,
    const std::wstring& ext) {
    // Thử tên cơ bản trước
    std::wstring candidate = baseName + ext;
    if (!FileExists(candidate)) {
        return candidate;
    }

    // Nếu đã tồn tại, đánh số (1), (2), ...
    int index = 1;
    while (true) {
        std::wstringstream ss;
        ss << baseName << L" (" << index << L")" << ext;
        candidate = ss.str();

        if (!FileExists(candidate)) {
            return candidate;
        }

        ++index;
    }
}

bool CaptureScreenAndSave(std::wstring* outFilePath,
    const std::wstring& baseName,
    const std::wstring& ext,
    bool openAfterSave) {
    // 1) Chụp màn hình
    HBITMAP hBitmap = CaptureScreenToHBitmap();
    if (!hBitmap) {
        return false;
    }

    // 2) Sinh tên file không trùng
    std::wstring filePath = GetUniqueScreenshotFileName(baseName, ext);

    // 3) Lưu xuống PNG
    bool ok = SaveHBitmapToPng(hBitmap, filePath);

    // Giải phóng HBITMAP
    DeleteObject(hBitmap);

    if (!ok) {
        return false;
    }

    // 4) Nếu caller cần biết đường dẫn
    if (outFilePath) {
        *outFilePath = filePath;
    }

    // 5) Nếu muốn mở ngay ảnh vừa lưu
    if (openAfterSave) {
        ShellExecuteW(
            NULL,
            L"open",
            filePath.c_str(),
            NULL,
            NULL,
            SW_SHOWNORMAL
        );
    }

    return true;
}
