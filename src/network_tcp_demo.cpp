#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <vector>

#include "network/NetworkHost.hpp"
#include "network/NetworkClient.hpp"
#include "network/TcpServer.hpp"
#include "network/TcpSession.hpp"
#include "network/MessageTypes.hpp"

using namespace tetris::net;
using namespace std::chrono_literals;

int main(int argc, char* argv[]) {
    if (argc == 1) {
        // --- HOST MODE ---
        const uint16_t port = 4000;
        std::cout << "[HOST] Starting TCP server on port " << port << "..." << std::endl;

        NetworkHost host(MultiplayerConfig{}); // default config

        TcpServer server(port, [&host](INetworkSessionPtr session) {
            std::cout << "[HOST] New session connected!" << std::endl;
            host.addClient(session);
        });

        server.start();

        std::cout << "[HOST] Waiting for clients. Press Enter to start the game..." << std::endl;
        std::cin.get();

        std::cout << "[HOST] Starting match!" << std::endl;
        host.startMatch();

        // --- Simple polling loop ---
        for (int tick = 0; tick < 20; ++tick) {
            host.poll();

            auto inputs = host.consumeInputQueue();
            for (auto& input : inputs) {
                std::cout << "[HOST] Received input from player " << input.playerId
                          << " Action: " << static_cast<int>(input.action) << std::endl;
            }

            std::this_thread::sleep_for(500ms);
        }

        server.stop();
        std::cout << "[HOST] Demo finished." << std::endl;

    } else if (argc == 4) {
        // --- CLIENT MODE ---
        std::string hostIp = argv[1];
        std::string playerName = argv[2];
        uint16_t port = std::stoi(argv[3]);  // Get the port from the argument

        std::cout << "[CLIENT] Connecting to host at " << hostIp << ":" << port
                  << " as player '" << playerName << "'..." << std::endl;

        auto session = TcpSession::createClient(hostIp, port);
        if (!session) {
            std::cerr << "[CLIENT] Failed to connect!" << std::endl;
            return 1;
        }

        NetworkClient client(session, playerName);

        // Notify when the match starts
        bool firstUpdate = true;
        client.setStateUpdateHandler([&](const StateUpdate& update) {
            if (firstUpdate) {
                std::cout << "[CLIENT] Game started!" << std::endl;
                firstUpdate = false;
            }
            std::cout << "[CLIENT] State update received. Tick: " << update.serverTick
                      << " Players: " << update.players.size() << std::endl;
        });

        client.setMatchResultHandler([](const MatchResult& result) {
            std::cout << "[CLIENT] Match ended for player " << result.playerId
                      << " Outcome: " << static_cast<int>(result.outcome) << std::endl;
        });

        client.start();

        // Keep the client running while host polls
        while (true) {
            std::this_thread::sleep_for(500ms);
            // For a real game, you'd consume inputs here
        }

    } else {
        std::cerr << "Usage:\n  Host: " << argv[0] << "\n  Client: "
                  << argv[0] << " <host_ip> <player_name> <port>\n";
        return 1;
    }

    return 0;
}
