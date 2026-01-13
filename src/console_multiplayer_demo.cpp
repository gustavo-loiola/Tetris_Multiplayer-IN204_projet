#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <vector>
#include <string>

#ifdef _WIN32
#include <conio.h> // para _kbhit() e _getch()
#else
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
int _kbhit() {
    static bool initialized = false;
    if (!initialized) {
        termios term;
        tcgetattr(STDIN_FILENO, &term);
        term.c_lflag &= ~ICANON;
        tcsetattr(STDIN_FILENO, TCSANOW, &term);
        setbuf(stdin, nullptr);
        initialized = true;
    }
    int bytesWaiting;
    ioctl(STDIN_FILENO, FIONREAD, &bytesWaiting);
    return bytesWaiting;
}
int _getch() { return getchar(); }
#endif

#include "network/NetworkHost.hpp"
#include "network/NetworkClient.hpp"
#include "network/TcpServer.hpp"
#include "network/TcpSession.hpp"
#include "network/MessageTypes.hpp"
#include "controller/InputAction.hpp"

using namespace tetris::net;
using namespace tetris::controller;
using namespace std::chrono_literals;

// Mapeia teclas para InputAction
InputAction mapKeyToAction(char key) {
    switch (key) {
        case 'a': return InputAction::MoveLeft;
        case 'd': return InputAction::MoveRight;
        case 's': return InputAction::SoftDrop;
        case 'w': return InputAction::HardDrop;
        case 'q': return InputAction::RotateCCW;
        case 'e': return InputAction::RotateCW;
        case 'p': return InputAction::PauseResume;
        default:  return InputAction::PauseResume;
    }
}

int main(int argc, char* argv[]) {
    if (argc == 1) {
        // === HOST MODE ===
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

        // --- Host polling loop ---
        for (int tick = 0; tick < 50; ++tick) {
            host.poll();

            auto inputs = host.consumeInputQueue();
            for (auto& input : inputs) {
                std::cout << "[HOST] Received input from player " << input.playerId
                          << " Action: " << static_cast<int>(input.action) << std::endl;
            }

            std::this_thread::sleep_for(200ms);
        }

        server.stop();
        std::cout << "[HOST] Demo finished." << std::endl;

    } else if (argc == 3) {
        // === CLIENT MODE ===
        std::string hostIp = argv[1];
        std::string playerName = argv[2];
        const uint16_t port = 4000;

        std::cout << "[CLIENT] Connecting to host " << hostIp << ":" << port
                  << " as " << playerName << "..." << std::endl;

        auto session = TcpSession::createClient(hostIp, port);
        if (!session) {
            std::cerr << "[CLIENT] Failed to connect!" << std::endl;
            return 1;
        }

        NetworkClient client(session, playerName);

        client.setStateUpdateHandler([&](const StateUpdate& update) {
            static bool firstUpdate = true;
            if (firstUpdate) {
                std::cout << "[CLIENT] Game started!" << std::endl;
                firstUpdate = false;
            }
            std::cout << "[CLIENT] Tick: " << update.serverTick
                      << " Players: " << update.players.size() << std::endl;
        });

        client.setMatchResultHandler([](const MatchResult& result) {
            std::cout << "[CLIENT] Match ended for player " << result.playerId
                      << " Outcome: " << static_cast<int>(result.outcome)
                      << " Score: " << result.finalScore << std::endl;
        });

        client.start();

        // --- Client input loop ---
        while (true) {
            if (_kbhit()) {
                char c = _getch();
                auto action = mapKeyToAction(c);
                client.sendInput(action, 0); // tick 0 para demo
            }
            std::this_thread::sleep_for(50ms);
        }

    } else {
        std::cerr << "Usage:\n"
                  << "  Host: " << argv[0] << "\n"
                  << "  Client: " << argv[0] << " <host_ip> <player_name>\n";
        return 1;
    }

    return 0;
}
