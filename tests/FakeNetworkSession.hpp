#pragma once
// FakeNetworkSession.hpp
//
// Test utility used by network unit tests.
// - Implements INetworkSession without real sockets.
// - Captures outbound messages via send()
// - Can simulate inbound messages (immediate or queued)
// - Can simulate disconnects via disconnect()
//
// Tip: Prefer queueIncoming()+poll() when you want a more realistic
// “event loop” delivery. Use injectIncoming() for simple unit tests.

#include <vector>
#include <deque>
#include <optional>
#include <utility>

#include "network/INetworkSession.hpp"
#include "network/MessageTypes.hpp"

class FakeNetworkSession : public tetris::net::INetworkSession {
public:
    using Message = tetris::net::Message;

    // Capture outgoing messages (host/client -> "network")
    void send(const Message& msg) override {
        sentMessages.push_back(msg);
    }

    // Flush queued inbound messages (more realistic than immediate inject)
    void poll() override {
        if (!m_connected) return;
        while (!m_incoming.empty()) {
            auto msg = std::move(m_incoming.front());
            m_incoming.pop_front();
            if (m_handler) m_handler(msg);
        }
    }

    void setMessageHandler(MessageHandler handler) override {
        m_handler = std::move(handler);
    }

    bool isConnected() const override {
        return m_connected;
    }

    // --------------------
    // Test helpers
    // --------------------

    // Queue an incoming message and deliver it on poll()
    void queueIncoming(const Message& msg) {
        m_incoming.push_back(msg);
    }

    // Deliver an incoming message immediately (keeps old behavior)
    void injectIncoming(const Message& msg) {
        if (m_connected && m_handler) m_handler(msg);
    }

    // Simulate a disconnect (after this, isConnected() == false)
    void disconnect() {
        m_connected = false;
    }

    // Count how many outbound messages of a given kind were sent
    std::size_t countKind(tetris::net::MessageKind k) const {
        std::size_t n = 0;
        for (const auto& m : sentMessages) {
            if (m.kind == k) ++n;
        }
        return n;
    }

    // Return the last outbound message of a given kind, if any
    std::optional<Message> lastOfKind(tetris::net::MessageKind k) const {
        for (auto it = sentMessages.rbegin(); it != sentMessages.rend(); ++it) {
            if (it->kind == k) return *it;
        }
        return std::nullopt;
    }

    // Outbound messages captured from send()
    std::vector<Message> sentMessages;

private:
    bool m_connected{true};
    std::deque<Message> m_incoming;
    MessageHandler m_handler;
};
