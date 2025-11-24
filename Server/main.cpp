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

#include "ProcessModule.h"

// Tu dong link thu vien socket cua Windows
#pragma comment(lib, "ws2_32.lib")

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

//------------------------------------------------------------------------------
// MODULE 2
//------------------------------------------------------------------------------

void DoShutdown()
{
    std::cout << "Executing Shutdown command...\n";
    // Lenh CMD: tat may (/s) sau 15 giay (/t 15)
    system("shutdown /s /t 15");
}

void DoRestart()
{
    std::cout << "Executing Restart command...\n";
    // Lenh CMD: khoi dong lai (/r) sau 15 giay (/t 15)
    system("shutdown /r /t 15");
}

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
            // 1. Chuyen du lieu tu Buffer sang String
            std::string command = beast::buffers_to_string(buffer_.data());

            std::cout << "RECEIVED (Bytes: " << bytes_transferred << "): "
                << command << "\n";

            // 2. Xu ly lenh 
            if (command == "shutdown")
            {
                // Tra loi Client
                ws_.text(true);
                ws_.write(net::buffer("Server: OK, shutting down..."));

                // Goi ham tat may
                DoShutdown();
            }
            else if (command == "restart")
            {
                ws_.text(true);
                ws_.write(net::buffer("Server: OK, restarting..."));

                // Goi ham restart
                DoRestart();
            }
            else
            {
                // Lenh la
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