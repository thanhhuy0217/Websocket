#pragma once
#include <string>

class FileTransfer {
public:
    // Hàm xử lý yêu cầu tải file
    // Input: Đường dẫn file
    // Output: Chuỗi protocol ("FILE_DOWNLOAD ...") hoặc thông báo lỗi ("Server: Error ...")
    static std::string HandleDownloadRequest(const std::string& filepath);
};