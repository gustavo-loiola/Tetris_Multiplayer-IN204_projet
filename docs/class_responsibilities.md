# Class Responsibilities – Multiplayer Tetris (IN204)

This document describes the main classes of the project, their responsibilities, and how they collaborate. It justifies the OOP decomposition, SOLID principles, and extension points required by IN204.

## 1. Design Principles

* **Separation of concerns**: core gameplay rules are isolated from UI and networking.
* **Single Responsibility**: each class has a narrow, explicit role.
* **Dependency direction**: UI and networking depend on core, never the opposite.
* **Evolutivity**: add new modes/protocol features/front-ends without rewriting the engine.
* **Modern C++**: RAII, no raw ownership, STL containers, const-correctness.

---

## 2. Core (Game Logic) – `tetris_core`

### 2.1 `Board`

**Responsibility**

* Owns the grid (locked blocks).
* Validates placement and collisions.
* Clears lines and compacts the grid.

**Collaborates with**

* `GameState` (reads/writes board cells, line clears).
* `Tetromino` (collision tests using block coordinates).

**Why it exists**

* Centralizes all grid invariants (no duplication of collision/clear logic).

---

### 2.2 `Tetromino`

**Responsibility**

* Represents a piece type (I/O/T/J/L/S/Z) and rotation state.
* Provides absolute block coordinates for rendering and collision.

**Why it exists**

* Encapsulates shape/rotation logic so the rest of the system stays simpler.

---

### 2.3 `TetrominoFactory`

**Responsibility**

* Generates pieces (random selection; can be extended to “bag”/seeded RNG).

**Pattern**

* Factory

**Why it exists**

* Makes generation replaceable and testable (determinism for tests or replays).

---

### 2.4 `GameState`

**Responsibility**

* Owns the complete game session state for one board:

  * board + active piece + spawn/lock rules
  * score/level integration
  * status: Running / Paused / GameOver
* Applies actions if valid (move/rotate/drop) and performs locking/clearing/scoring.

**Important note**

* In multiplayer, the host holds the authoritative `GameState`(s).
* The client **does not** simulate `GameState` during network play; it renders snapshots.

---

### 2.5 `ScoreManager`

**Responsibility**

* Encapsulates scoring policy (line clears, bonuses).

---

### 2.6 `LevelManager`

**Responsibility**

* Encapsulates level progression and gravity parameters.

---

### 2.7 `IMatchRules`, `TimeAttackRules`, `SharedTurnRules`

**Responsibility**

* Define match-level policies that are *not* per-action mechanics:

  * when a match ends (time limit / game over)
  * how to determine winner
  * (SharedTurns) whose turn it is and how it advances

**Pattern**

* Strategy

**Current implemented behavior**

* **TimeAttack**: ends on time limit or death; winner by survival/score.
* **SharedTurns**: one shared board; turn changes based on **pieces per turn**.

**Why it exists**

* Allows adding new multiplayer modes without rewriting UI + networking.

---

## 3. Controller Layer – `controller/`

### 3.1 `InputAction`

**Responsibility**

* Defines the “intent vocabulary” (MoveLeft/Right, RotateCW/CCW, SoftDrop, HardDrop, PauseResume…).
* Acts as the common interface between UI, networking, and core.

**Why it exists**

* Prevents SDL key codes or GUI details from leaking into gameplay logic.

---

### 3.2 `GameController`

**Responsibility**

* Runs the time-based update loop:

  * gravity timing
  * locking/spawning triggers
  * pause/resume
* Applies `InputAction` on demand via `handleAction()`.
* Updates the attached `GameState` via `update(dt)`.

**Why it exists**

* Keeps time mechanics out of `GameState`, improving testability and separation.

---

## 4. Networking – `tetris_net`

> The project uses an **authoritative host** model.
> Clients send inputs; host simulates; clients render snapshots.

### 4.1 `MessageTypes`

**Responsibility**

* Defines protocol vocabulary and payload structures.
* Current key messages:

  * `JoinRequest`, `JoinAccept`
  * `StartGame`
  * `InputActionMessage`
  * `StateUpdate` (snapshot DTO)
  * `MatchResult`
  * `Error`
  * (Optional depending on your current branch) `PlayerLeft`

**Why it exists**

* Keeps all wire contracts explicit and shared by host/client code.

---

### 4.2 `Serialization`

**Responsibility**

* Converts `Message` ↔ single-line text representation.
* Handles escaping and parsing.
* Ensures TCP stream framing is reliable (message boundaries).

**Why it exists**

* TCP is a stream; the project enforces a clear “one line = one message” framing.

---

### 4.3 `INetworkSession`

**Responsibility**

* Abstract interface for a duplex message channel:

  * send `Message`
  * poll for reads
  * set message handler callback
  * report connection state

**Why it exists**

* Decouples higher-level host/client logic from socket implementation.

---

### 4.4 `TcpSession`

**Responsibility**

* Concrete TCP implementation of `INetworkSession`.
* Reads from socket, assembles lines, calls message handler with deserialized `Message`.
* Sends serialized messages.
* Detects disconnections and reports `isConnected()`.

**Why it exists**

* Isolates OS-specific socket code and stream parsing.

---

### 4.5 `TcpServer`

**Responsibility**

* Listens on a TCP port.
* Accepts incoming connections.
* Wraps accepted sockets as `TcpSession` and passes them to the host layer.

---

### 4.6 `NetworkHost`

**Responsibility**

* Maintains connected clients (`PlayerInfo` with id/session/name).
* Receives and queues client `InputActionMessage` (host consumes them during simulation).
* Sends `JoinAccept` after a `JoinRequest`.
* Sends `StartGame` to clients at match start (and at rematch restart).
* Broadcasts:

  * periodic `StateUpdate` snapshots
  * `MatchResult`
  * disconnect signals (if your protocol uses `PlayerLeft`)

**Rematch role (as implemented)**

* Tracks “rematch readiness” via a client resending `JoinRequest` after match end.
* Exposes queries like `anyClientReadyForRematch()` and utilities like `clearRematchFlags()`.

**Why it exists**

* Centralizes server-side networking policy and connection bookkeeping.

---

### 4.7 `NetworkClient`

**Responsibility**

* Owns the client-side networking:

  * sends `JoinRequest`
  * sends `InputActionMessage`
  * receives `StartGame`, `StateUpdate`, `MatchResult`, etc.
* Stores “last received” messages for polling by the GUI.
* Provides *consume* semantics (`consumeStateUpdate()`, etc.) so UI does not reuse stale messages.

**Why it exists**

* Isolates network IO from UI logic and ensures thread-safe access.

---

### 4.8 `StateUpdateMapper`

**Responsibility**

* Converts internal core state to DTO snapshots:

  * board → `BoardDTO` (cells, occupancy, colorIndex)
  * player state → `PlayerStateDTO` (score/level/alive + board)
* Allows extending snapshots with extra HUD fields (time left, turn data) without touching core logic.

---

## 5. GUI – `tetris_gui_sdl` (SDL2 + ImGui)

### 5.1 `Application`

**Responsibility**

* Owns the SDL window and ImGui context.
* Runs the main loop:

  * poll SDL events
  * call current `Screen::handleEvent`, `Screen::update`, `Screen::render`
* Provides screen transitions (`setScreen()`).

---

### 5.2 `Screen`

**Responsibility**

* Abstract base for UI states.
* Defines the interface:

  * `handleEvent(app, event)`
  * `update(app, dt)`
  * `render(app)`

**Why it exists**

* Implements a clean state machine for GUI navigation.

---

### 5.3 `StartScreen`

**Responsibility**

* Main menu.
* Lets the user choose single-player or multiplayer configuration.

---

### 5.4 `MultiplayerConfigScreen`

**Responsibility**

* Collects configuration:

  * host vs join
  * address/port
  * game mode + parameters
* Important UX rule implemented:

  * **Client cannot override game parameters**; host settings are authoritative.

---

### 5.5 `LobbyScreen`

**Responsibility**

* Host side:

  * starts `TcpServer`
  * creates `NetworkHost`
  * waits for at least one client
  * “Start Match” sends `StartGame` and transitions to game screen
* Client side:

  * creates `TcpSession` to host
  * creates `NetworkClient`
  * sends `JoinRequest`
  * waits for `StartGame`
  * updates `cfg_` from `StartGame` before entering game

---

### 5.6 `MultiplayerGameScreen`

**Responsibility**

* Runs the match UI:

  * **Host**: simulates core state(s) + applies remote inputs + broadcasts snapshots.
  * **Client**: sends inputs, renders authoritative snapshots.
* Renders:

  * TimeAttack: two boards + two scoreboards + time-left HUD
  * SharedTurns: one board + turn HUD (turn player, pieces left)
* End-of-match:

  * Host decides winner and broadcasts `MatchResult`.
  * Both show match overlay with results and options.
* Rematch:

  * Host waits until both agree; restarts by sending new `StartGame`.
  * Client requests rematch (re-JoinRequest) and waits for new `StartGame`.
* Disconnect UX:

  * Client detects snapshot timeout → “HOST DISCONNECTED” overlay with button.
  * Host detects client disconnected → ends match and shows “Opponent disconnected”.

**Why it exists**

* Concentrates multiplayer UX logic while keeping core and network independent.

---

## 6. Collaboration Summary

* **Core (`tetris_core`)**: authoritative gameplay rules.
* **Controller (`GameController`)**: timing + gravity + action application.
* **Networking (`tetris_net`)**:

  * clients send actions
  * host simulates + broadcasts snapshots/results
* **GUI (`tetris_gui_sdl`)**:

  * maps input to `InputAction`
  * host applies locally; client sends to host
  * rendering uses either local `GameState` (host/offline) or snapshots (client)