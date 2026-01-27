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

* Owns the grid of locked blocks (occupied cells).
* Validates placement and collisions.
* Locks tetromino blocks into the grid.
* Clears full lines and compacts the grid.

**Collaborates with**

* `GameState` (reads/writes cells, line clears, game over conditions).
* `Tetromino` (collision tests using block coordinates).

**Why it exists**

* Centralizes all grid invariants (bounds, collisions, line clearing), avoiding duplication across UI and controllers.

---

### 2.2 `Tetromino`

**Responsibility**

* Represents a piece type (I/O/T/J/L/S/Z) with a rotation state and an origin.
* Produces the absolute block coordinates used for:

  * collision checks (core)
  * rendering (UI)

**Why it exists**

* Encapsulates shape and rotation logic so board and controller remain simple.

---

### 2.3 `TetrominoFactory`

**Responsibility**

* Generates new tetrominoes (random selection) for spawning.
* Owns an RNG (`std::mt19937`) to allow future deterministic generation or testing improvements.

**Pattern**

* Factory

**Why it exists**

* Isolates piece generation policy from gameplay rules, improving testability and evolutivity.

---

### 2.4 `GameState`

**Responsibility**

* Owns the complete state of one Tetris session:

  * `Board`
  * active tetromino + next tetromino
  * `ScoreManager` and `LevelManager`
  * lifecycle (`NotStarted`, `Running`, `Paused`, `GameOver`)
* Implements the **core gameplay rules and legality**:

  * moves left/right
  * rotations (CW/CCW)
  * soft/hard drop
  * locking the active piece
  * line clearing and compaction
  * scoring + level updates after line clears
* Exposes a **discrete control API** to the controller layer.

**Important note**

* `GameState` is an **active domain object**: it encapsulates both state and core gameplay rules.
* Time scheduling (gravity timing) is intentionally handled by `GameController`.

**Multiplayer note**

* The host is authoritative and holds the authoritative `GameState`(s).
* The client does not simulate `GameState` during network play; it renders snapshots.

---

### 2.5 `ScoreManager`

**Responsibility**

* Encapsulates scoring policy:

  * points for line clears depending on level
  * small bonuses for soft/hard drop distance

**Why it exists**

* Keeps scoring rules isolated and replaceable without touching board movement logic.

---

### 2.6 `LevelManager`

**Responsibility**

* Encapsulates level progression:

  * total cleared line tracking
  * level increments on thresholds
* Provides gravity parameters via a gravity interval function.

**Why it exists**

* Separates difficulty progression from `GameState` mechanics.

---

### 2.7 `IMatchRules`, `TimeAttackRules`, `SharedTurnRules` (Match Strategy)

**Responsibility**

* Define match-level policies that are **not** local per-cell mechanics:

  * match start synchronization (`onMatchStart`)
  * match end conditions (time limit, survival, etc.)
  * winner decision (win/lose/draw)
  * (SharedTurns) current turn owner and turn switching rules

**Interface design choice**

* Rules operate on lightweight `PlayerSnapshot` DTOs (id, score, alive),
  rather than depending directly on `GameState`.
* This reduces coupling and keeps match logic independent from board internals.

**Pattern**

* Strategy

**Current implemented behavior**

* **TimeAttackRules**

  * ends after a configured tick/time limit (or when deaths occur, depending on implementation)
  * winner decided by survival and/or score comparison
* **SharedTurnRules**

  * one shared board
  * turn changes after a fixed number of locked pieces (`piecesPerTurn`)
  * exposes the current allowed player via `currentPlayer()` for host logic

**Why it exists**

* Allows adding new multiplayer modes without rewriting the UI and networking layers.

---

## 3. Controller Layer – `controller/`

### 3.1 `InputAction`

**Responsibility**

* Defines a small, explicit vocabulary of player intents:

  * MoveLeft/MoveRight
  * RotateCW/RotateCCW
  * SoftDrop/HardDrop
  * PauseResume

**Why it exists**

* Prevents SDL key codes, GUI events, or network representation from leaking into core gameplay logic.

---

### 3.2 `GameController`

**Responsibility**

* Runs the **time-based update loop**:

  * accumulates elapsed time
  * triggers gravity ticks based on `GameState::gravityIntervalMs()`
* Applies discrete player actions via `handleAction()`.
* Delegates all legality/rules to `GameState`.

**Ownership rule**

* The controller does **not** own `GameState` (stores a reference); the caller manages lifetime.

**Why it exists**

* Keeps time scheduling and gravity mechanics separate from the gameplay rules, improving testability and reuse
  (single-player, host simulation, AI-driven play).

---

## 4. Networking – `tetris_net`

> The project uses a **host-authoritative** model:
> clients send inputs; host simulates; clients render snapshots.

### 4.1 `MessageTypes` (`Message`, `MessageKind`, payload structs)

**Responsibility**

* Defines protocol vocabulary and payload structures.
* Current key messages include:

  * `JoinRequest`, `JoinAccept`
  * `StartGame`
  * `InputActionMessage`
  * `StateUpdate` (snapshot DTO including HUD fields)
  * `MatchResult`
  * `PlayerLeft`
  * `RematchDecision`
  * `KeepAlive`
  * `ErrorMessage`

**Why it exists**

* Keeps all wire contracts explicit, versionable, and shared by host/client.

---

### 4.2 `Serialization`

**Responsibility**

* Converts `Message` ↔ single-line UTF-8 representation.
* Implements message framing (“one line = one message”) and parsing.
* Returns parse failures as `std::nullopt` (caller decides how to handle malformed input).

**Why it exists**

* TCP is a stream; this enforces reliable boundaries and isolates protocol encoding/decoding.

---

### 4.3 `INetworkSession`

**Responsibility**

* Abstract duplex channel used by host and client:

  * send `Message`
  * register a message handler callback
  * query connection state

**Why it exists**

* Decouples high-level multiplayer logic from the concrete socket implementation.

---

### 4.4 `TcpSession`

**Responsibility**

* Concrete TCP implementation of `INetworkSession`.
* Runs a background read thread that:

  * reads bytes from the socket
  * splits into lines (`'\n'`)
  * deserializes each line into a `Message`
  * invokes the registered message handler
* Sends serialized messages to the peer.

**Why it exists**

* Isolates OS-level socket code and stream parsing, keeping host/client logic clean.

---

### 4.5 `TcpServer`

**Responsibility**

* Listens on a TCP port.
* Accepts incoming connections.
* Creates `TcpSession` instances and exposes them via a callback.
* Runs accept loop in a background thread.

---

### 4.6 `NetworkHost`

**Responsibility**

* Host-side networking endpoint:

  * accepts sessions (`addClient`)
  * manages lobby player info (id/name/connected)
  * handles join handshake and assigns `PlayerId`
  * collects incoming `InputActionMessage` into an input queue
  * broadcasts protocol messages (`StartGame`, `StateUpdate`, `MatchResult`, etc.)
* Detects disconnections and broadcasts `PlayerLeft`.
* Implements rematch handshake:

  * tracks `RematchDecision` from clients
  * exposes readiness queries (`allConnectedClientsReadyForRematch`, etc.)
* Sends `KeepAlive` periodically to preserve client liveness during non-gameplay phases.

**Why it exists**

* Centralizes server-side connection bookkeeping and protocol handling (SRP).

---

### 4.7 `NetworkClient`

**Responsibility**

* Client-side networking endpoint:

  * sends `JoinRequest` and input actions
  * receives `StartGame`, `StateUpdate`, `MatchResult`, `PlayerLeft`, `Error`
  * exposes “last received” and “consume once” APIs for the UI
* Provides optional event handlers (callbacks) for StartGame / StateUpdate / MatchResult.
* Tracks liveness (`timeSinceLastHeard`) to support UI disconnect detection.

**Threading note**

* Network callbacks may occur on a background thread (from `TcpSession`).
* The client protects shared state with a mutex to allow safe polling/consumption from the UI thread.

**Why it exists**

* Isolates network IO and protocol details from the GUI, enabling clean and testable UI logic.

---

### 4.8 `StateUpdateMapper`

**Responsibility**

* Converts authoritative `core::GameState` into network DTOs:

  * `PlayerStateDTO` including `BoardDTO`, score, level, alive state
* Keeps mapping logic outside of core gameplay so the core stays network-agnostic.

**Why it exists**

* Implements a clean **DTO mapping boundary** between core and protocol.

---

### 4.9 `HostGameSession`

**Responsibility**

* High-level host orchestrator combining:

  * `NetworkHost` (protocol + connections)
  * `IMatchRules` (mode strategy)
* Starts the match and initializes rule state.
* Forwards piece-lock events to rules (`onPieceLocked`).
* Calls rule update regularly and returns match results when finished.
* Delegates `StateUpdate` broadcasting through `NetworkHost`.

**Why it exists**

* Separates “match policy” from both the low-level network protocol and the concrete simulation loop.

---

### 4.10 `HostLoop`

**Responsibility**

* Glue component that coordinates:

  * `HostGameSession` (rules + networking)
  * authoritative `GameState` instances
  * `GameController` instances (gravity scheduling + action application)
* On each step:

  * consumes queued remote inputs and forwards them to the correct controller
  * updates controllers with elapsed time
  * detects newly locked pieces and notifies rules
  * periodically builds and broadcasts `StateUpdate`

**Ownership rule**

* `HostLoop` does not own `GameState` or `GameController`; it operates on pointers provided by its caller.

**Why it exists**

* Makes the multiplayer host simulation loop explicit, testable, and independent of GUI.

---

## 5. GUI – `tetris_gui_sdl` (SDL2 + ImGui)

### 5.1 `Application`

**Responsibility**

* Owns the SDL window/renderer and ImGui context.
* Runs the main loop:

  * poll SDL events
  * call `Screen::handleEvent`, `Screen::update`, `Screen::render`
* Manages screen transitions.

---

### 5.2 `Screen`

**Responsibility**

* Abstract base for UI states with:

  * event handling
  * update
  * render

**Why it exists**

* Implements a clean UI state machine (menu, lobby, gameplay, etc.).

---

### 5.3 `StartScreen`

**Responsibility**

* Entry menu (single-player / multiplayer / exit).
* Provides lightweight animated background to improve UX.

---

### 5.4 `SinglePlayerScreen`

**Responsibility**

* Owns a local `GameState` + `GameController`.
* Maps keyboard inputs into `InputAction`.
* Renders:

  * board
  * active piece
  * next piece preview
  * HUD (score/level/pause/game over)

---

### 5.5 `MultiplayerConfigScreen`

**Responsibility**

* Collects multiplayer configuration:

  * role (host/join)
  * address/port
  * player name
  * mode + parameters (time limit / pieces per turn)

---

### 5.6 `LobbyScreen`

**Responsibility**

* Host side:

  * starts `TcpServer`
  * creates `NetworkHost`
  * accepts clients
  * lets host start the match
* Client side:

  * connects via `TcpSession::createClient`
  * creates `NetworkClient`
  * sends `JoinRequest`
  * waits for `StartGame`

---

### 5.7 `MultiplayerGameScreen`

**Responsibility**

* Runs the multiplayer match UI.
* **Host path**:

  * simulates authoritative state(s) locally using `GameState` + `GameController`
  * consumes remote inputs and applies them appropriately
  * broadcasts `StateUpdate` snapshots periodically
  * determines results (directly or via host-side match logic) and sends `MatchResult`
* **Client path**:

  * sends input actions to the host
  * renders the latest `StateUpdate` snapshot (no client physics)
* Implements match overlays:

  * win/lose/draw
  * rematch flow
  * disconnect handling

**Why it exists**

* Concentrates multiplayer UX logic while keeping core gameplay and networking isolated and reusable.

---

## 6. Collaboration Summary

* **Core (`tetris_core`)**: authoritative gameplay mechanics and state.
* **Controller (`controller/`)**: gravity timing and discrete input application.
* **Networking (`tetris_net`)**:

  * host collects inputs, simulates, broadcasts snapshots, decides results
  * client sends inputs and renders snapshots
* **GUI (`tetris_gui_sdl`)**:

  * maps SDL input to `InputAction`
  * host applies locally; client sends to host
  * renders either local state (single-player/host) or snapshots (client)

---
