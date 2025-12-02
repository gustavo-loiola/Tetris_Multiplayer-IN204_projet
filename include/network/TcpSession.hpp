#pragma once

#include <memory>
#include <string>
#include <atomic> 
#include <thread>  
#include <mutex> 

#include "network/INetworkSession.hpp"

namespace tetris::net {

class TcpServer;

/// Concrete INetworkSession using a TCP socket and line-based protocol.
/// It runs a background thread that:
///   - reads bytes from the socket
///   - splits on '\n'
///   - parses each line as a serialized Message
///   - invokes the registered MessageHandler.
/// `poll()` is a no-op; the session is event-driven.
class TcpSession : public INetworkSession {
public:
    /// Create a client session connected to the given host:port.
    /// Returns nullptr on failure.
    static INetworkSessionPtr createClient(const std::string& host, std::uint16_t port);

    ~TcpSession() override;

    void send(const Message& msg) override;
    void poll() override; // no-op for this implementation
    void setMessageHandler(MessageHandler handler) override;
    bool isConnected() const override { return m_connected; }

    // Non-copyable, non-movable
    TcpSession(const TcpSession&) = delete;
    TcpSession& operator=(const TcpSession&) = delete;

private:
    // Constructed internally with an already-connected socket.
    explicit TcpSession(int socketFd);

    friend class TcpServer;

    void readLoop();
    void closeSocket();

    int m_socket{-1};
    std::atomic<bool> m_connected{false};
    std::thread m_thread;

    MessageHandler m_handler;
    std::mutex m_handlerMutex;
};

} // namespace tetris::net
