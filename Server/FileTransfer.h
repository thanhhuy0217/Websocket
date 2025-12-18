#pragma once
#include <string>

class FileTransfer {
public:
    // Xử lý yêu cầu tải file: Đọc file -> Mã hóa Base64 -> Trả về chuỗi để gửi Socket
    static std::string HandleDownloadRequest(const std::string& filepath);

    // [MỚI] Liệt kê danh sách file/thư mục: Trả về chuỗi định dạng để Client phân tích
    static std::string ListDirectory(const std::string& path);
};