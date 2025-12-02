#include "network/TcpSession.hpp"
#include "network/Serialization.hpp"

#include <atomic>
#include <thread>
#include <mutex>
#include <string>
#include <vector>
#include <cstring>
#include <iostream>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    using socket_t = SOCKET;
    static constexpr socket_t INVALID_SOCKET_FD = INVALID_SOCKET;
    #define CLOSE_SOCKET closesocket
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    using socket_t = int;
    static constexpr socket_t INVALID_SOCKET_FD = -1;
    #define CLOSE_SOCKET ::close
#endif

namespace {

#ifdef _WIN32
void ensure_winsock_initialized()
{
    static std::once_flag flag;
    std::call_once(flag, [] {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            std::cerr << "WSAStartup failed: " << result << "\n";
        }
    });
}
#else
void ensure_winsock_initialized() {}
#endif

} // namespace

namespace tetris::net {

TcpSession::TcpSession(int socketFd)
    : m_socket(socketFd)
    , m_connected(true)
{
    // Start background read loop
    m_thread = std::thread(&TcpSession::readLoop, this);
}

TcpSession::~TcpSession()
{
    m_connected = false;
    closeSocket();
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

INetworkSessionPtr TcpSession::createClient(const std::string& host, std::uint16_t port)
{
    ensure_winsock_initialized();

    socket_t sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET_FD) {
        return nullptr;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);

    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
        CLOSE_SOCKET(sock);
        return nullptr;
    }

    if (::connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        CLOSE_SOCKET(sock);
        return nullptr;
    }

    return INetworkSessionPtr(new TcpSession(static_cast<int>(sock)));
}

void TcpSession::send(const Message& msg)
{
    if (!m_connected) return;

    const std::string line = serialize(msg) + "\n";
    const char* data = line.c_str();
    std::size_t total = line.size();

    while (total > 0 && m_connected) {
        int sent = ::send(m_socket, data, static_cast<int>(total), 0);
        if (sent <= 0) {
            m_connected = false;
            break;
        }
        total -= static_cast<std::size_t>(sent);
        data  += sent;
    }
}

void TcpSession::poll()
{
    // No-op: this implementation is event-driven via a background thread.
}

void TcpSession::setMessageHandler(MessageHandler handler)
{
    std::lock_guard<std::mutex> lock(m_handlerMutex);
    m_handler = std::move(handler);
}

void TcpSession::readLoop()
{
    std::string buffer;
    buffer.reserve(4096);

    char tmp[1024];

    while (m_connected) {
        int received = ::recv(m_socket, tmp, sizeof(tmp), 0);
        if (received <= 0) {
            // Connection closed or error
            m_connected = false;
            break;
        }

        buffer.append(tmp, tmp + received);

        // Extract complete lines (terminated by '\n')
        std::size_t pos = 0;
        while (true) {
            auto newlinePos = buffer.find('\n', pos);
            if (newlinePos == std::string::npos)
                break;

            std::string line = buffer.substr(pos, newlinePos - pos);
            pos = newlinePos + 1;

            if (!line.empty()) {
                auto msgOpt = deserialize(line);
                if (msgOpt) {
                    MessageHandler handlerCopy;
                    {
                        std::lock_guard<std::mutex> lock(m_handlerMutex);
                        handlerCopy = m_handler;
                    }
                    if (handlerCopy) {
                        handlerCopy(*msgOpt);
                    }
                }
            }
        }

        // Discard processed part
        if (pos > 0) {
            buffer.erase(0, pos);
        }
    }

    closeSocket();
}

void TcpSession::closeSocket()
{
    if (m_socket != static_cast<int>(INVALID_SOCKET_FD)) {
        CLOSE_SOCKET(m_socket);
        m_socket = static_cast<int>(INVALID_SOCKET_FD);
    }
}

} // namespace tetris::net
