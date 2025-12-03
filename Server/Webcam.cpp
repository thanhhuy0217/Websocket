#include "Webcam.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

// --- CAU HINH TEN CAMERA ---
// Luu y: Thay doi ten nay cho dung voi may cua ban
const std::string CAMERA_NAME = "USB2.0 HD UVC WebCam";

std::vector<char> CaptureWebcam(int durationSeconds)
{
    std::string outputFile = "webcam_temp.mp4";
    std::vector<char> fileData;

    // Cau lenh FFmpeg de quay video
    // -y: ghi de file
    // -preset ultrafast: nen toc do cao
    std::string command = "ffmpeg -f dshow -i video=\"" + CAMERA_NAME + "\" -t " +
        std::to_string(durationSeconds) +
        " -c:v libx264 -preset ultrafast -pix_fmt yuv420p " + // <--- THÊM VÀO ÐÂY
        outputFile + " -y > nul 2>&1";

    std::cout << "[WEBCAM] Recording for " << durationSeconds << " seconds...\n";

    // Goi lenh he thong
    int result = system(command.c_str());

    if (result != 0) {
        std::cerr << "[ERROR] FFmpeg failed. Check camera name.\n";
        return fileData;
    }

    // Doc file video vao RAM
    std::ifstream file(outputFile, std::ios::binary | std::ios::ate);
    if (file.is_open()) {
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        fileData.resize(size);
        file.read(fileData.data(), size);
        file.close();

        std::cout << "[SUCCESS] Video captured (" << size << " bytes).\n";
    }
    else {
        std::cerr << "[ERROR] Could not open output video file.\n";
    }

    return fileData;
}