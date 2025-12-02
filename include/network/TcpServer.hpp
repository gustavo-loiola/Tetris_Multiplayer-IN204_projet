#pragma once

#include <functional>
#include <thread>
#include <atomic>

#include "network/INetworkSession.hpp"

namespace tetris::net {

/// Simple TCP server that listens on a port and creates TcpSession
/// instances for each accepted connection. It calls a user-provided
/// callback with INetworkSessionPtr for each new client.
class TcpServer {
public:
    using NewSessionCallback = std::function<void(INetworkSessionPtr)>;

    TcpServer(std::uint16_t port, NewSessionCallback onNewSession);
    ~TcpServer();

    /// Start the accept loop in a background thread.
    void start();

    /// Stop the server and join the background thread.
    void stop();

    bool isRunning() const { return m_running; }

private:
    void acceptLoop();

    std::uint16_t m_port;
    NewSessionCallback m_onNewSession;

    int m_listenSocket{-1};
    std::atomic<bool> m_running{false};
    std::thread m_thread;
};

} // namespace tetris::net
