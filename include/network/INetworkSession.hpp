#pragma once

#include <functional>
#include <memory>
#include "network/MessageTypes.hpp"

namespace tetris::net {

class INetworkSession {
public:
    using MessageHandler = std::function<void(const Message&)>;

    virtual ~INetworkSession() = default;

    // Send a message to the remote peer.
    virtual void send(const Message& msg) = 0;

    // Poll underlying sockets once; host/client can call this from their update loop.
    virtual void poll() = 0;

    // Set callback invoked whenever a complete Message is received.
    virtual void setMessageHandler(MessageHandler handler) = 0;

    // Host or client may query whether session is still alive.
    virtual bool isConnected() const = 0;
};

using INetworkSessionPtr = std::shared_ptr<INetworkSession>;

} // namespace tetris::net
