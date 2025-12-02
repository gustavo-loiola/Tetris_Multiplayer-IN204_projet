#pragma once

#include <vector>

#include "network/INetworkSession.hpp"

class FakeNetworkSession : public tetris::net::INetworkSession {
public:
    using Message = tetris::net::Message;

    void send(const Message& msg) override {
        sentMessages.push_back(msg);
    }

    void poll() override {
        // No-op for fake session.
    }

    void setMessageHandler(MessageHandler handler) override {
        m_handler = std::move(handler);
    }

    bool isConnected() const override {
        return true;
    }

    /// Helper for tests: simulate an incoming message from host.
    void injectIncoming(const Message& msg) {
        if (m_handler) {
            m_handler(msg);
        }
    }

    std::vector<Message> sentMessages;

private:
    MessageHandler m_handler;
};
