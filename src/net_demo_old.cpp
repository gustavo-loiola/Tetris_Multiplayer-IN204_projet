#include <iostream>
#include <thread>
#include <chrono>
#include <memory>

#include "network/NetworkHost.hpp"
#include "network/NetworkClient.hpp"
#include "network/TcpServer.hpp"
#include "network/TcpSession.hpp"
#include "network/MessageTypes.hpp"

using namespace tetris::net;
using namespace std::chrono_literals;

int main() {
    const uint16_t port = 4000;

    // --- Host ---
    std::cout << "[HOST] Starting TCP server..." << std::endl;
    NetworkHost host(MultiplayerConfig{}); // default config

    // TCP server that will create sessions and add to host
    TcpServer server(port, [&host](INetworkSessionPtr session) {
        std::cout << "[HOST] New session connected!" << std::endl;
        host.addClient(session);
    });

    server.start();

    // --- Client 1 ---
    std::this_thread::sleep_for(100ms); // small delay to ensure server is ready
    std::cout << "[CLIENT1] Connecting..." << std::endl;
    auto session1 = TcpSession::createClient("127.0.0.1", port);
    if (!session1) {
        std::cerr << "Client 1 failed to connect!" << std::endl;
        return 1;
    }
    NetworkClient client1(session1, "Alice");
    client1.setStateUpdateHandler([](const StateUpdate& update) {
        std::cout << "[CLIENT1] State update received. Tick: " << update.serverTick
                  << " Players: " << update.players.size() << std::endl;
    });
    client1.setMatchResultHandler([](const MatchResult& result) {
        std::cout << "[CLIENT1] Match ended for player " << result.playerId
                  << " Outcome: " << static_cast<int>(result.outcome) << std::endl;
    });
    client1.start();

    // --- Client 2 ---
    std::cout << "[CLIENT2] Connecting..." << std::endl;
    auto session2 = TcpSession::createClient("127.0.0.1", port);
    if (!session2) {
        std::cerr << "Client 2 failed to connect!" << std::endl;
        return 1;
    }
    NetworkClient client2(session2, "Bob");
    client2.setStateUpdateHandler([](const StateUpdate& update) {
        std::cout << "[CLIENT2] State update received. Tick: " << update.serverTick
                  << " Players: " << update.players.size() << std::endl;
    });
    client2.setMatchResultHandler([](const MatchResult& result) {
        std::cout << "[CLIENT2] Match ended for player " << result.playerId
                  << " Outcome: " << static_cast<int>(result.outcome) << std::endl;
    });
    client2.start();

    // --- Simulate host polling loop ---
    for (int tick = 0; tick < 10; ++tick) {
        host.poll();

        // Simulate inputs
        std::vector<InputActionMessage> inputs = host.consumeInputQueue();
        for (auto& input : inputs) {
            std::cout << "[HOST] Received input from player " << input.playerId
                      << " Action: " << static_cast<int>(input.action) << std::endl;
        }

        std::this_thread::sleep_for(500ms);
    }

    server.stop();
    std::cout << "[DEMO] Network demo finished." << std::endl;
    return 0;
}
