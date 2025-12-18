#include "FileTransfer.h"
#include <fstream>
#include <vector>
#include <windows.h>
#include <wincrypt.h> 
#include <iostream>
#include <sstream>
#include <iomanip>

// Link thư viện mã hóa của Windows (Có sẵn, không cần cài thêm)
#pragma comment(lib, "Crypt32.lib")

// --- CÁC HÀM HỖ TRỢ CHUYỂN ĐỔI UNICODE (ĐỂ HỖ TRỢ TIẾNG VIỆT) ---

// Chuyển từ std::string (UTF-8) sang std::wstring (UTF-16/Unicode)
// Dùng để gọi các API của Windows (như FindFirstFileW)
std::wstring Utf8ToWstring(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

// Chuyển từ std::wstring (UTF-16) sang std::string (UTF-8)
// Dùng để gửi dữ liệu qua Socket (Socket thường dùng UTF-8)
std::string WstringToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// --- LOGIC TẢI FILE (DOWNLOAD) ---
std::string FileTransfer::HandleDownloadRequest(const std::string& filepath) {
    // 1. Chuyển đường dẫn sang Unicode để hỗ trợ tiếng Việt
    std::wstring wideFilepath = Utf8ToWstring(filepath);

    // 2. Mở file ở chế độ Binary
    std::ifstream file(wideFilepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return "Server: Error! File not found or locked: " + filepath;
    }

    // 3. Lấy kích thước file
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size <= 0) return "Server: Error! File is empty.";

    // 4. Đọc toàn bộ nội dung file vào buffer
    std::vector<char> buffer(size);
    if (!file.read(buffer.data(), size)) return "Server: Error reading file.";

    // 5. Mã hóa Base64 bằng thư viện Crypt32 của Windows (tối ưu tốc độ)
    DWORD outLen = 0;
    // Bước 1: Lấy kích thước buffer cần thiết
    CryptBinaryToStringA((const BYTE*)buffer.data(), (DWORD)size, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, NULL, &outLen);

    std::string base64Str(outLen, '\0');
    // Bước 2: Mã hóa thật
    CryptBinaryToStringA((const BYTE*)buffer.data(), (DWORD)size, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &base64Str[0], &outLen);

    // 6. Đóng gói theo giao thức tự định nghĩa: "FILE_DOWNLOAD <FileName> <Base64Data>"
    // Lấy tên file từ đường dẫn đầy đủ
    size_t lastSlash = filepath.find_last_of("\\/");
    std::string filename = (lastSlash == std::string::npos) ? filepath : filepath.substr(lastSlash + 1);

    return "FILE_DOWNLOAD " + filename + " " + base64Str;
}

// Thay thế hàm ListDirectory cũ trong FileTransfer.cpp
std::string FileTransfer::ListDirectory(const std::string& path) {
    std::stringstream ss;

    // 1. NẾU YÊU CẦU LÀ "MyComputer" -> LIỆT KÊ CÁC Ổ ĐĨA
    if (path == "MyComputer" || path.empty()) {
        ss << "DIR_LIST\n";
        ss << "MyComputer\n"; // Path hiện tại là MyComputer

        // Lấy danh sách ổ đĩa (Vd: "C:\0D:\0\0")
        char buffer[256];
        DWORD len = GetLogicalDriveStringsA(sizeof(buffer), buffer);

        if (len > 0) {
            char* ptr = buffer;
            while (*ptr) {
                std::string drive = ptr; // Vd: "C:\"
                // Gửi về với tag [DRIVE] để Client hiển thị icon ổ cứng
                ss << "[DRIVE]|" << drive << "\n";

                // Nhảy đến ổ tiếp theo (do cách nhau bằng null byte)
                ptr += strlen(ptr) + 1;
            }
        }
        return ss.str();
    }

    // 2. LOGIC CŨ: LIỆT KÊ FILE TRONG THƯ MỤC
    std::string searchPath = path;

    // Fix lỗi: Nếu người dùng gửi "C:" (thiếu backslash) -> thêm thành "C:\"
    if (searchPath.back() != '\\') searchPath += "\\";

    std::string wildCard = searchPath + "*";
    std::wstring wWildCard = Utf8ToWstring(wildCard);

    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(wWildCard.c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE) {
        // Nếu lỗi, thử trả về danh sách ổ đĩa để người dùng không bị kẹt
        return "DIR_LIST\nMyComputer\nServer: Error opening path. Showing drives instead.\n";
    }

    ss << "DIR_LIST\n";
    ss << searchPath << "\n";

    do {
        std::wstring wFileName = findData.cFileName;
        if (wFileName == L"." || wFileName == L"..") continue;

        std::string fileName = WstringToUtf8(wFileName);

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            ss << "[DIR]|" << fileName << "\n";
        }
        else {
            long long fileSize = ((long long)findData.nFileSizeHigh << 32) | findData.nFileSizeLow;
            double sizeKB = fileSize / 1024.0;
            std::string sizeStr;
            if (sizeKB > 1024) sizeStr = std::to_string((int)(sizeKB / 1024)) + " MB";
            else {
                std::stringstream temp;
                temp << std::fixed << std::setprecision(1) << sizeKB << " KB";
                sizeStr = temp.str();
            }
            ss << "[FILE]|" << fileName << "|" << sizeStr << "\n";
        }

    } while (FindNextFileW(hFind, &findData));

    FindClose(hFind);
    return ss.str();
}