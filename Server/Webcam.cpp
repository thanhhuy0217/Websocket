#define _CRT_SECURE_NO_WARNINGS
#include "Webcam.h"
#include <cstdlib>      
#include <fstream>      
#include <iostream>
#include <string>
#include <array>        
#include <cstdio>       
#include <memory>       
#include <algorithm>

// --- Ham tu dong tim ten Camera ---
std::string GetCameraNameAuto()
{
    std::string cameraName = "";

    // 1. Goi lenh FFmpeg de no liet ke tat ca thiet bi ra
    // -hide_banner: An bot thong tin rac
    // 2>&1: Lay tat ca chu ma no in ra man hinh
    std::string cmd = "ffmpeg -list_devices true -f dshow -i dummy -hide_banner 2>&1";

    std::array<char, 4096> buffer; // Bo dem lon de chua du thong tin
    std::string result = "";

    // Chay lenh ngam va doc ket qua tra ve
    std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd.c_str(), "r"), _pclose);

    if (!pipe) {
        std::cout << "[WEBCAM] Failed to run detection command.\n";
        return "";
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    // Output cua FFmpeg luon co dang: ... "Ten Camera" (video) ...
    // Chung ta se bam vao chu "(video)" de tim ra ten

    std::string videoTag = "(video)";
    size_t videoPos = result.find(videoTag);

    if (videoPos != std::string::npos) {
        // 1. Tim dau ngoac kep dong (") dung ngay truoc chu (video)
        size_t quoteEnd = result.rfind("\"", videoPos);

        if (quoteEnd != std::string::npos) {
            // 2. Tim dau ngoac kep mo (") dung truoc dau ngoac kep dong
            size_t quoteStart = result.rfind("\"", quoteEnd - 1);

            if (quoteStart != std::string::npos) {
                // 3. Cat lay doan chu nam giua 2 dau ngoac kep -> Day chinh la Ten Camera
                cameraName = result.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
            }
        }
    }

    return cameraName;
}

std::vector<char> CaptureWebcam(int durationSeconds)
{
    std::vector<char> fileData;
    std::string outputFile = "webcam_temp.mp4";

    // 1. Tu dong tim ten Camera (Khong can dien tay nua)
    std::string cameraName = GetCameraNameAuto();

    if (cameraName.empty()) {
        std::cerr << "[ERROR] Could not auto-detect any webcam! (Check ffmpeg.exe)\n";
        return fileData;
    }

    std::cout << "[WEBCAM] Detected Camera: [" << cameraName << "]\n";

    // 2. Tao cau lenh quay video
    // -pix_fmt yuv420p: Bat buoc de xem duoc tren Windows
    std::string command = "ffmpeg -f dshow -i video=\"" + cameraName + "\" -t " +
        std::to_string(durationSeconds) +
        " -c:v libx264 -preset ultrafast -pix_fmt yuv420p " +
        outputFile + " -y > nul 2>&1";

    std::cout << "[WEBCAM] Recording " << durationSeconds << "s...\n";

    // 3. Chay lenh
    int result = system(command.c_str());

    if (result != 0) {
        std::cerr << "[ERROR] Recording failed. Camera might be in use by another app.\n";
        return fileData;
    }

    // 4. Doc file ket qua
    std::ifstream file(outputFile, std::ios::binary | std::ios::ate);
    if (file.is_open()) {
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        if (size > 0) {
            fileData.resize(size);
            file.read(fileData.data(), size);
            std::cout << "[SUCCESS] Video captured (" << size << " bytes).\n";
        }
        else {
            std::cerr << "[ERROR] Output file is empty.\n";
        }

        file.close();
        std::remove(outputFile.c_str());
    }
    else {
        std::cerr << "[ERROR] Output file not found.\n";
    }

    return fileData;
}