#include "FileTransfer.h"
#include <fstream>
#include <vector>
#include <windows.h>
#include <wincrypt.h> 
#include <iostream>

#pragma comment(lib, "Crypt32.lib")

// Hàm chuyển đổi UTF-8 sang UTF-16 
std::wstring Utf8ToWstring(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

std::string FileTransfer::HandleDownloadRequest(const std::string& filepath) {
    // 1. Chuyển đổi đường dẫn sang Unicode để mở file tiếng Việt
    std::wstring wideFilepath = Utf8ToWstring(filepath);

    std::ifstream file(wideFilepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return "Server: Error! File not found or locked: " + filepath;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size <= 0) return "Server: Error! File is empty.";

    std::vector<char> buffer(size);
    if (!file.read(buffer.data(), size)) return "Server: Error reading file.";

    // 2. Mã hóa Base64
    DWORD outLen = 0;
    if (!CryptBinaryToStringA((const BYTE*)buffer.data(), (DWORD)size, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, NULL, &outLen)) return "Error calc size";

    std::string base64Data(outLen, '\0');
    if (!CryptBinaryToStringA((const BYTE*)buffer.data(), (DWORD)size, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &base64Data[0], &outLen)) return "Error encoding";

    // 3. Lấy tên file
    std::string filename = filepath.substr(filepath.find_last_of("/\\") + 1);

    return "FILE_DOWNLOAD|" + filename + "|" + base64Data;
}