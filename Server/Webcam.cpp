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
#include <thread> // Cho sleep

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
    std::string rawFile = "webcam_raw.mp4";   // File tam chua du lieu tho
    std::string finalFile = "webcam_final.mp4"; // File hoan chinh sau khi sua

    std::string cameraName = GetCameraNameAuto();

    if (cameraName.empty()) {
        std::cerr << "[WEBCAM] Error: Could not auto-detect webcam.\n";
        return fileData;
    }

    int actualDuration = durationSeconds + 1;

    std::cout << "[WEBCAM] Recording " << durationSeconds << "s (Buffer: " << actualDuration << "s)...\n";

    std::string recordCmd = "ffmpeg -f dshow -i video=\"" + cameraName + "\" -t " +
        std::to_string(actualDuration) +
        " -c:v libx264 -preset ultrafast -pix_fmt yuv420p " +
        "-g 30 " +
        "-movflags frag_keyframe+empty_moov+default_base_moof " +
        rawFile + " -y > nul 2>&1";

    system(recordCmd.c_str());

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::cout << "[WEBCAM] Processing/Repairing video...\n";
    std::string repairCmd = "ffmpeg -y -i " + rawFile + " -c copy " + finalFile + " > nul 2>&1";
    system(repairCmd.c_str());

    // 4. Doc file KET QUA (Final)
    std::ifstream file(finalFile, std::ios::binary | std::ios::ate);
    if (file.is_open()) {
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        if (size > 0) {
            fileData.resize(size);
            file.read(fileData.data(), size);
            std::cout << "[WEBCAM] Success! Captured " << size << " bytes.\n";
        }
        else {
            std::cerr << "[WEBCAM] Warning: Output file empty.\n";
        }
        file.close();
    }
    else {
        std::cerr << "[WEBCAM] Error: Could not open output file.\n";
    }

    // Don dep
    std::remove(rawFile.c_str());
    std::remove(finalFile.c_str());

    return fileData;
}
