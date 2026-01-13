# Architecture – Multiplayer Tetris (IN204)

This document describes the high-level architecture (“architecture gros grains”) of the Multiplayer Tetris project, implemented in modern C++ with a modular, evolutive design. The project currently supports a **SDL2 + ImGui** GUI front-end and a **TCP host-authoritative multiplayer** model.

## 1. Goals

The architecture aims to:

* Keep the **core game engine independent** from UI and networking.
* Support incremental extension of gameplay through **pluggable match rules** (Strategy).
* Allow multiple front-ends (console, SDL/ImGui) without impacting the engine.
* Provide online multiplayer with an **authoritative host** and a lightweight message protocol.
* Keep networking message handling isolated (DTO mapping + serialization), so it can evolve without touching the core.

---

## 2. Repository Modules (Macro-Architecture)

### 2.1 Core (Game Logic) – `tetris_core`

**Location**

* Headers: `include/core/*`, `include/controller/*`
* Sources: `src/core/*`, `src/controller/*`

**Responsibilities**

* Represent the board, pieces, and internal state (`GameState`).
* Enforce movement/rotation/collision/line clearing.
* Maintain lifecycle (`Running`, `Paused`, `GameOver`).
* Compute scoring and level progression.
* Provide match rule strategies for multiplayer modes.
* Provide the update loop through `GameController`.

**Key classes/files**

* `Board` (`include/core/Board.hpp`, `src/core/Board.cpp`)
* `Tetromino`, `TetrominoFactory` (`include/core/Tetromino*.hpp`, `src/core/Tetromino*.cpp`)
* `GameState` (`include/core/GameState.hpp`, `src/core/GameState.cpp`)
* `ScoreManager`, `LevelManager`
* **Match rules strategy**

  * `IMatchRules` (`include/core/IMatchRules.hpp`)
  * `TimeAttackRules` (`src/core/TimeAttackRules.cpp`)
  * `SharedTurnRules` (`src/core/SharedTurnRules.cpp`) — current behavior: *turn-based by pieces/turn*
* **Controller**

  * `GameController` (`include/controller/GameController.hpp`, `src/controller/GameController.cpp`)
  * `InputAction` (`include/controller/InputAction.hpp`)

**Core invariants**

* `tetris_core` has **no dependency** on SDL/ImGui or networking.
* The core remains the single source of truth for gameplay legality: UI never “teleports pieces”.

---

### 2.2 Networking – `tetris_net`

**Location**

* Headers: `include/network/*`
* Sources: `src/network/*`

**Responsibilities**

* Provide multiplayer over TCP using a **host authoritative** model:

  * Clients send `InputActionMessage`.
  * Host applies actions to authoritative game state(s).
  * Host broadcasts `StateUpdate` snapshots periodically (20 Hz).
  * Host broadcasts `MatchResult` when game ends.
* Provide protocol definitions + serialization.
* Provide mapping between core state and “wire DTOs”.

**Key classes/files**

* Session abstraction:

  * `INetworkSession` (`include/network/INetworkSession.hpp`)
* TCP transport:

  * `TcpServer` (`src/network/TcpServer.cpp`) — accepts sessions
  * `TcpSession` (`src/network/TcpSession.cpp`) — client and server session, line-based message IO
* Host side orchestration:

  * `NetworkHost` (`include/network/NetworkHost.hpp`, `src/network/NetworkHost.cpp`)
  * (optional/auxiliary depending on your codebase) `HostGameSession`, `HostLoop`
* Client side:

  * `NetworkClient` (`include/network/NetworkClient.hpp`, `src/network/NetworkClient.cpp`)
* Protocol + serialization:

  * `MessageTypes` (`include/network/MessageTypes.hpp`)
  * `Serialization` (`src/network/Serialization.cpp`)
  * `StateUpdateMapper` (`src/network/StateUpdateMapper.cpp`) — maps `core::GameState/Board` to `BoardDTO/PlayerStateDTO`

**Transport**

* TCP sockets (links `ws2_32` on Windows).
* Messages are serialized to **single-line UTF-8** (no trailing newline inside message), sent as lines.

---

### 2.3 GUI (SDL2 + ImGui) – `tetris_gui_sdl`

**Location**

* Headers: `include/gui_sdl/*`
* Sources: `src/gui_sdl/*`

**Responsibilities**

* Provide game navigation and rendering via SDL2 + ImGui.
* Implement the full multiplayer flow:

  * Config selection (host/join, address/port, mode params).
  * Lobby (host waits, client joins).
  * Multiplayer game screen renders boards + scoreboards + timer/turn HUD.
* Translate keyboard input into `InputAction`:

  * Host applies locally to core controller.
  * Client sends to host through `NetworkClient`.
* Client is *render-only* for physics:

  * it renders snapshots (`StateUpdate`) and displays results from host.

**Key screens**

* `StartScreen` — entry menu
* `MultiplayerConfigScreen` — host/join + parameters UI
* `LobbyScreen` — connection status and match start
* `MultiplayerGameScreen` — gameplay rendering:

  * TimeAttack: two boards (host and opponent)
  * SharedTurns: shared board + turn HUD (pieces left + whose turn)
  * Match overlay: winner/loser/draw, rematch agreement, disconnect warnings (“Host disconnected” / “Opponent disconnected”)

**UI constraints**

* GUI should never re-implement rules; it only:

  * sends `InputAction` or applies via `GameController`
  * renders either local `GameState` (host/offline) or `StateUpdate` (client)

---

### 2.4 Console Executable – `tetris_console` (debug/legacy)

**Location**

* `src/main_console.cpp`

**Responsibilities**

* Minimal runner useful for debugging `tetris_core` without GUI.

---

## 3. Main Execution Flows

### 3.1 Single-player (SDL GUI)

1. `StartScreen` → user selects Single Player
2. Screen owns a `GameState` + `GameController`
3. Update loop calls `GameController::update(dt)`
4. Keyboard events map to `InputAction` → `GameController::handleAction`
5. Rendering draws board + active piece

*(Your current focus is multiplayer, but this flow remains the conceptual baseline.)*

---

### 3.2 Multiplayer – Host-authoritative model

#### Host flow

1. User selects Multiplayer → Host in `MultiplayerConfigScreen`
2. `LobbyScreen`:

   * starts `TcpServer`
   * creates `NetworkHost`
   * accepts clients via `host_->addClient(session)`
3. When host clicks **Start Match**:

   * host sends `StartGame` to all clients (`NetworkHost::startMatch()`)
   * host enters `MultiplayerGameScreen`
4. During gameplay:

   * host runs core simulation via `GameController::update(dt)`
   * host consumes client inputs (`host_->consumeInputQueue()`)
   * host applies them to the correct state:

     * TimeAttack: inputs control the opponent’s game
     * SharedTurns: host accepts inputs only from current `turnPlayerId`
   * host broadcasts snapshots (`StateUpdate`) at fixed rate (20 Hz)
5. End-of-match:

   * host decides outcome (time limit, deaths, scores)
   * host broadcasts `MatchResult`
   * host UI displays overlay + rematch logic

#### Client flow

1. User selects Multiplayer → Join in `MultiplayerConfigScreen`
2. `LobbyScreen`:

   * connects via `TcpSession::createClient`
   * creates `NetworkClient(session, name)`
   * sends `JoinRequest`
   * waits for `StartGame`
3. When `StartGame` received:

   * client enters `MultiplayerGameScreen`
4. During gameplay:

   * client **does not run physics**
   * keyboard actions → `NetworkClient::sendInput()`
   * rendering uses last received `StateUpdate`
5. End-of-match:

   * client receives `MatchResult`
   * overlay shows result + rematch request logic
6. Disconnect handling:

   * if no snapshots for a timeout → client shows “Host disconnected” overlay with button to return to menu

---

## 4. Multiplayer Modes

### 4.1 TimeAttack

* Two independent boards:

  * Host board (player 1)
  * Opponent board (player 2)
* End conditions:

  * time limit reached, or
  * one/both players reach `GameOver`
* Winner:

  * if exactly one died → other wins
  * else compare scores → win/lose/draw

### 4.2 SharedTurns (turn-based by pieces)

* One shared board (host authoritative).
* Only the player whose `turnPlayerId` matches may act.
* Turns switch when the current player has placed `piecesPerTurn` pieces.
* UI shows:

  * current turn player
  * pieces left in turn

---

## 5. Evolutivity Points (Extension Hooks)

* **Match rules strategy** (`IMatchRules`) allows adding new modes without modifying UI/network heavily.
* **Front-end replacement**: SDL/ImGui is one frontend; others can be added with minimal changes to core/net.
* **Protocol extension**: message definitions + serialization are isolated in `tetris_net`.
* **Snapshot DTO mapping** is isolated (`StateUpdateMapper`), allowing protocol changes (delta compression, richer state).

---

## 6. Design Patterns Used

* **Factory**: `TetrominoFactory`
* **Strategy**: `IMatchRules` → `TimeAttackRules`, `SharedTurnRules`
* **MVC-inspired separation**:

  * Model: `GameState`, `Board`, `Tetromino`
  * Controller: `GameController`
  * View: SDL/ImGui screens (`Screen` subclasses)
* **RAII & Modern C++**: `std::unique_ptr`, `std::shared_ptr`, standard containers
* **Host-authoritative networking** (common multiplayer architecture)

---

## 7. Build Targets

CMake targets:

* `tetris_core` (library)
* `tetris_net` (library)
* `tetris_console` (executable)
* `tetris_gui_sdl` (executable)

Dependencies:

* SDL2
* Dear ImGui
* WinSock (`ws2_32`) on Windows
