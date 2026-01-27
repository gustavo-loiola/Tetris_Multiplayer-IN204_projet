#include "network/TcpServer.hpp"
#include "network/TcpSession.hpp"

#include <iostream>
#include <mutex>   // for std::once_flag, std::call_once

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
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

TcpServer::TcpServer(std::uint16_t port, NewSessionCallback onNewSession)
    : m_port(port)
    , m_onNewSession(std::move(onNewSession))
{
}

TcpServer::~TcpServer()
{
    stop();
}

void TcpServer::start()
{
    if (m_running) return;

    ensure_winsock_initialized();

    socket_t listenSock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listenSock == INVALID_SOCKET_FD) {
        std::cerr << "TcpServer: failed to create socket\n";
        return;
    }

    int opt = 1;
    ::setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(m_port);

    if (::bind(listenSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "TcpServer: bind failed\n";
        CLOSE_SOCKET(listenSock);
        return;
    }

    if (::listen(listenSock, SOMAXCONN) < 0) {
        std::cerr << "TcpServer: listen failed\n";
        CLOSE_SOCKET(listenSock);
        return;
    }

    m_listenSocket = static_cast<int>(listenSock);
    m_running = true;

    m_thread = std::thread(&TcpServer::acceptLoop, this);
}

void TcpServer::stop()
{
    if (!m_running) return;

    m_running = false;

    if (m_listenSocket != static_cast<int>(INVALID_SOCKET_FD)) {
        CLOSE_SOCKET(m_listenSocket);
        m_listenSocket = static_cast<int>(INVALID_SOCKET_FD);
    }

    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void TcpServer::acceptLoop()
{
    while (m_running) {
        sockaddr_in clientAddr{};
        socklen_t addrLen = sizeof(clientAddr);

        socket_t clientSock =
            ::accept(m_listenSocket, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);

        if (clientSock == INVALID_SOCKET_FD) {
            if (!m_running) {
                break;
            }
            std::cerr << "TcpServer: accept failed\n";
            continue;
        }

        // Wrap the client socket in a TcpSession and notify the callback.
        auto session = INetworkSessionPtr(
            new TcpSession(static_cast<int>(clientSock))
        );

        if (m_onNewSession) {
            m_onNewSession(std::move(session));
        }
    }
}

} // namespace tetris::net
