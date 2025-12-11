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
    std::string cmd = "ffmpeg -list_devices true -f dshow -i dummy -hide_banner 2>&1";

    std::array<char, 4096> buffer;
    std::string result = "";

    std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd.c_str(), "r"), _pclose);

    if (!pipe) {
        return "";
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    std::string videoTag = "(video)";
    size_t videoPos = result.find(videoTag);

    if (videoPos != std::string::npos) {
        size_t quoteEnd = result.rfind("\"", videoPos);
        if (quoteEnd != std::string::npos) {
            size_t quoteStart = result.rfind("\"", quoteEnd - 1);
            if (quoteStart != std::string::npos) {
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

    std::string cameraName = GetCameraNameAuto();

    if (cameraName.empty()) {
        std::cerr << "[ERROR] Could not auto-detect any webcam! (Check ffmpeg.exe)\n";
        return fileData;
    }

    std::cout << "[WEBCAM] Start Recording: [" << cameraName << "] for " << durationSeconds << "s...\n";

    // Cau lenh quay video
    std::string command = "ffmpeg -f dshow -i video=\"" + cameraName + "\" -t " +
        std::to_string(durationSeconds) +
        " -c:v libx264 -preset ultrafast -pix_fmt yuv420p " +
        outputFile + " -y > nul 2>&1";

    int result = system(command.c_str());

    // 4. Doc file ket qua (Du la quay het gio hay bi kill)
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
            // Chi bao loi neu file rong
            std::cerr << "[WARNING] Output file is empty (maybe stopped too fast).\n";
        }

        file.close();
        std::remove(outputFile.c_str());
    }
    else {
        std::cerr << "[ERROR] Output file not found.\n";
    }

    return fileData;
}

void StopWebcam()
{
    std::cout << "[WEBCAM] Stopping recording...\n";
    // Lenh nay se kill ffmpeg, lam cho CaptureWebcam tiep tuc chay xuong phan doc file
    system("taskkill /F /IM ffmpeg.exe > nul 2>&1");
}