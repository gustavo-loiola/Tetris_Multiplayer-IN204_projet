# Architecture – Multiplayer Tetris (IN204)

This document describes the high-level architecture ("architecture gros grains") of the Multiplayer Tetris project, implemented in modern C++ with a modular, evolutive design.

## 1. Goals

The architecture aims to:

- Keep the **core game engine independent** from UI and networking.
- Support incremental extension of gameplay through **pluggable match rules** (Strategy).
- Allow multiple front-ends (console, wxWidgets GUI; future SDL possible) without impacting the engine.
- Provide an online multiplayer model with an **authoritative host** and lightweight message protocol.

## 2. Repository Modules (Macro-Architecture)

### 2.1 Core (Game Logic) – `tetris_core`

**Location**
- Headers: `include/core/*`
- Sources: `src/core/*` (+ `src/controller/GameController.cpp` is linked into the core library)

**Responsibilities**
- Represent the board and pieces.
- Enforce movement, rotation, collision, and line clearing.
- Maintain game lifecycle (running/paused/game-over).
- Compute scoring and level progression.
- Provide match-level rules for multiplayer modes (Time Attack, Shared Turns).

**Key classes/files**
- `Board` (`include/core/Board.hpp`, `src/core/Board.cpp`)
- `Tetromino`, `TetrominoFactory` (`include/core/Tetromino*.hpp`, `src/core/Tetromino*.cpp`)
- `GameState` (`include/core/GameState.hpp`, `src/core/GameState.cpp`)
- `ScoreManager` (`include/core/ScoreManager.hpp`, `src/core/ScoreManager.cpp`)
- `LevelManager` (`include/core/LevelManager.hpp`, `src/core/LevelManager.cpp`)
- Match rules strategy:
  - `IMatchRules` (`include/core/IMatchRules.hpp`)
  - `TimeAttackRules` (`src/core/TimeAttackRules.cpp`)
  - `SharedTurnRules` (`src/core/SharedTurnRules.cpp`)
- Controller loop:
  - `GameController` (`include/controller/GameController.hpp`, `src/controller/GameController.cpp`)
  - `InputAction` (`include/controller/InputAction.hpp`)

> Design principle: `tetris_core` must have **no dependency** on wxWidgets or networking.

---

### 2.2 Networking – `tetris_net`

**Location**
- Headers: `include/network/*`
- Sources: `src/network/*`

**Responsibilities**
- Enable multiplayer between machines using TCP.
- Implement an authoritative host model:
  - Clients send input actions.
  - Host applies actions to authoritative game state(s).
  - Host sends state updates back to clients.
- Provide message definitions and serialization.

**Key classes/files**
- Session abstraction: `INetworkSession` (`include/network/INetworkSession.hpp`)
- Host side:
  - `NetworkHost` (`src/network/NetworkHost.cpp`)
  - `HostGameSession` (`src/network/HostGameSession.cpp`)
  - `HostLoop` (`src/network/HostLoop.cpp`)
  - `TcpServer` (`src/network/TcpServer.cpp`)
- Client side:
  - `NetworkClient` (`src/network/NetworkClient.cpp`)
  - `TcpSession` (`src/network/TcpSession.cpp`)
- Protocol + serialization:
  - `MessageTypes` (`include/network/MessageTypes.hpp`)
  - `Serialization` (`src/network/Serialization.cpp`)
  - `StateUpdateMapper` (`src/network/StateUpdateMapper.cpp`)

**Transport**
- TCP sockets (links `ws2_32` on Windows).

---

### 2.3 GUI – `tetris_gui` (wxWidgets)

**Location**
- Headers: `include/gui/*`
- Sources: `src/gui/*`

**Responsibilities**
- Provide graphical views and user interaction.
- Present a start menu and allow selecting:
  - Single-player
  - Multiplayer (host or join) + mode configuration
- Render local and remote boards.
- Translate keyboard input into `InputAction` and forward to controller or network client.

**Key UI components**
- Entry point: `TetrisApp`
- Navigation: `StartFrame`
- Single-player game window: `TetrisFrame`
- Panels:
  - `BoardPanel` (local board rendering)
  - `NextPiecePanel` (preview)
  - `RemoteBoardPanel` (remote board view in multiplayer)
- Multiplayer flow:
  - `MultiplayerConfigDialog` (host/join + mode config)
  - `MultiplayerFrame` (multiplayer gameplay view)

> UI should not re-implement game rules. It observes `GameState` and triggers actions via controller/network.

---

### 2.4 Console Executable – `tetris_console` (legacy/debug)

**Location**
- `src/main_console.cpp`

**Responsibilities**
- Provide a minimal runner useful for debugging the model/controller without the GUI.

---

## 3. Main Execution Flows

### 3.1 Single-player (GUI)
1. `StartFrame` → user selects **Single Player**
2. `TetrisFrame` created
3. `TetrisFrame` owns/uses a `GameController` + `GameState`
4. Timer loop calls `GameController::update(dt)`
5. Keyboard events map to `InputAction` and are applied via controller

### 3.2 Multiplayer (Authoritative host)
**Host**
1. User selects Multiplayer → Host
2. Host initializes `NetworkHost` and `HostLoop`
3. For each player: host maintains one authoritative `GameState` (or one shared depending on mode)
4. Host receives `InputAction` messages from clients
5. Host applies actions to authoritative state(s)
6. Host sends `StateUpdate` messages periodically or on change

**Client**
1. User selects Multiplayer → Join
2. Client connects using `NetworkClient`/`TcpSession`
3. Client sends join request + local `InputAction` events
4. Client receives `StateUpdate` and renders remote state

---

## 4. Evolutivity Points (Extension Hooks)

- **Match Rules Strategy** (`IMatchRules`) allows adding new modes without changing controller/UI significantly.
- **Front-end replacement**: GUI module can be replaced (e.g., SDL) while keeping `tetris_core` and `tetris_net`.
- **Protocol extension**: message types and mapping are isolated in `network/`.

---

## 5. Design Patterns Used

- **Factory**: `TetrominoFactory` creates randomized tetromino instances.
- **Strategy**: `IMatchRules` with `TimeAttackRules` and `SharedTurnRules`.
- **MVC-inspired separation**:
  - Model: `GameState`, `Board`, `Tetromino`
  - Controller: `GameController`
  - View: wxWidgets frames/panels
- **RAII & Modern C++**: standard containers and resource management; sockets wrapped in sessions.

---

## 6. Build Targets

CMake targets:
- `tetris_core` (library)
- `tetris_net` (library)
- `tetris_console` (executable)
- `tetris_gui` (executable, optional via `BUILD_GUI=ON`)

Dependencies:
- `wxwidgets` (via vcpkg manifest `vcpkg.json`)
- Catch2 (fetched via CMake `FetchContent` when tests enabled)
