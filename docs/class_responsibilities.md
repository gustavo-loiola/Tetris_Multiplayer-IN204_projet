# Class Responsibilities – Multiplayer Tetris (IN204)

This document explains the main classes of the project, their responsibilities, and how they collaborate. The goal is to justify the design choices (OOP decomposition, SOLID principles, patterns) and provide maintainable extension points, as required by IN204.

## 1. Design Principles

The project follows these principles:

- **Separation of concerns**: game rules (core) are isolated from UI and networking.
- **Single Responsibility**: each class has a narrow, clear purpose.
- **Dependency direction**: UI and networking depend on core, never the opposite.
- **Evolutivity**: new game modes and front-ends can be added without rewriting the engine.
- **Modern C++ practices**: RAII, no raw ownership, const-correctness, and use of STL.

---

## 2. Core (Model) – `tetris_core`

### 2.1 `Board`
**Responsibility**
- Owns the grid state (occupied cells, fixed blocks).
- Validates collisions and bounds.
- Applies line clear operations and compacts the board.

**Key behaviors**
- Query/set cell occupancy
- Check whether a tetromino can be placed at a position
- Detect full lines and clear them

**Why it exists**
- Centralizes all grid invariants and avoids scattering collision logic.

---

### 2.2 `Tetromino`
**Responsibility**
- Represents a tetromino shape and its rotation state.
- Provides the occupied block coordinates for a given rotation.

**Key behaviors**
- Rotate (90° increments)
- Enumerate blocks in local coordinates

**Why it exists**
- Encapsulates shape/rotation logic so Board and GameState stay simpler.

---

### 2.3 `TetrominoFactory`
**Responsibility**
- Generates tetromino instances according to the game rules (e.g., random selection).
- Can be extended to implement alternative generation strategies (bag randomizer, seeded RNG).

**Pattern**
- Factory

**Why it exists**
- Makes piece generation replaceable and testable.

---

### 2.4 `GameState`
**Responsibility**
- Owns the complete game session state for a player or shared match:
  - current board
  - active falling piece and its position
  - next piece (if applicable)
  - score/level state integration
  - paused/game-over flags
- Enforces lifecycle transitions (start/reset/pause/game over).

**Key behaviors**
- Spawn new piece
- Apply a player action (move/rotate/drop) if valid
- Lock piece when necessary and commit to the board
- Trigger scoring + level updates after line clears

**Why it exists**
- Acts as the authoritative "single source of truth" for the game world.

---

### 2.5 `ScoreManager`
**Responsibility**
- Computes score changes (especially line-clear bonuses).
- Tracks accumulated score for a session.

**Why it exists**
- Keeps scoring policy independent from gameplay mechanics and easy to validate with unit tests.

---

### 2.6 `LevelManager`
**Responsibility**
- Tracks the current level and progression conditions.
- Provides gravity/step timing parameters associated with level.

**Why it exists**
- Isolates progression rules and makes speed changes consistent.

---

## 3. Controller – `controller/`

### 3.1 `InputAction`
**Responsibility**
- Defines the set of player intents (left/right/rotate/drop/pause/etc.).
- Provides an input vocabulary shared by UI, console, and multiplayer messages.

**Why it exists**
- Prevents UI- or keycode-specific logic from leaking into the core.

---

### 3.2 `GameController`
**Responsibility**
- Runs the time-based game loop:
  - accumulates elapsed time
  - applies gravity steps at the proper interval
  - applies `InputAction`s received from the UI or (in host mode) from the network
- Coordinates lifecycle operations (start/pause/reset).

**Why it exists**
- Keeps time and update mechanics out of `GameState`, making `GameState` easier to test.

---

## 4. Match Rules (Multiplayer Modes) – Strategy Pattern

### 4.1 `IMatchRules`
**Responsibility**
- Defines a generic interface to evaluate match-level policies:
  - how the match starts/ends
  - how winners are determined
  - how turn ownership is handled (when relevant)
- Operates over one or more `GameState` instances.

**Pattern**
- Strategy

**Why it exists**
- Enables adding new multiplayer modes without rewriting networking or UI flows.

---

### 4.2 `TimeAttackRules`
**Responsibility**
- Implements a time-limited match:
  - one `GameState` per player
  - match ends when the timer expires
  - winner determined by score (tie-break policies can be added)

**Extension points**
- time limit configuration
- tie-breaker rules

---

### 4.3 `SharedTurnRules`
**Responsibility**
- Implements a shared-board match:
  - a single shared `GameState`
  - determines which player is currently allowed to act
  - switches turn based on a rule (e.g., every N pieces)

**Extension points**
- pieces-per-turn configuration
- alternative turn switching logic

---

## 5. Networking – `tetris_net`

> Multiplayer uses an **authoritative host**. Clients do not simulate the match rules; they only send inputs and render received state updates.

### 5.1 `MessageTypes`
**Responsibility**
- Defines the protocol vocabulary: join/start/ready/input/state update/results/errors.

**Status note**
- Some message types may exist but the UI flow (lobby/ready) is still under development.

---

### 5.2 `Serialization`
**Responsibility**
- Serializes/deserializes protocol messages to/from a byte stream suitable for TCP.
- Provides framing rules so messages are extracted correctly from the TCP stream.

**Why it exists**
- TCP is a stream: message boundaries must be encoded and decoded explicitly.

---

### 5.3 `TcpServer` / `TcpSession`
**Responsibility**
- Provide the low-level socket operations:
  - accept connections (server)
  - send/receive bytes (session)
- Detect disconnects and propagate errors upward.

**Why it exists**
- Keeps OS-specific socket code isolated from match logic.

---

### 5.4 `NetworkHost`
**Responsibility**
- Owns the host-side networking interface.
- Receives client messages (join/ready/input).
- Sends authoritative state updates and match events.

---

### 5.5 `NetworkClient`
**Responsibility**
- Owns the client-side networking interface.
- Sends join/ready/input messages to host.
- Sends ReadyUp.
- Receives state updates and forwards them to the UI.

---

### 5.6 `HostGameSession`
**Responsibility**
- Represents a single multiplayer match instance at the host:
  - keeps track of connected players
  - maintains authoritative `GameState`(s)
  - integrates `IMatchRules` evaluation
  - decides when to start and when to end the match
  - tracks ready flags

**Lobby intent (planned behavior)**
- Host creates a session and waits for at least one connection.
- Once connected, all players must mark themselves **Ready**.
- Match starts only when readiness conditions are satisfied.

---

### 5.7 `HostLoop`
**Responsibility**
- Drives the host update cycle:
  - processes incoming messages
  - applies inputs to game states
  - ticks controllers/timers
  - evaluates match rules
  - broadcasts state updates

**Why it exists**
- Concentrates the "server game loop" into one orchestrator.

---

### 5.8 `StateUpdateMapper`
**Responsibility**
- Converts between internal model structures (e.g., Board/GameState) and protocol-friendly data (DTO).
- Supports partial updates or full snapshots depending on implementation.

**Why it exists**
- Keeps serialization formats stable while allowing internal model evolution.

### Lobby + Ready Tracking (Planned)

To support a clear multiplayer UX, the host maintains a lobby phase before the match starts.

**Planned responsibilities**
- `HostGameSession`
  - tracks connected players (`PlayerId`, name)
  - tracks readiness flags (ready/not ready)
  - starts the match only when readiness conditions are met
- `NetworkClient`
  - sends join request and later sends ready status when user presses Ready
- GUI (`MultiplayerFrame` or a dedicated lobby panel)
  - displays participant list and readiness state
  - provides a Ready button
  - does not start the match itself (host remains authoritative)

**Planned protocol messages**
- `ReadyUp` (client → host): informs host that a player toggled readiness
- `LobbyUpdate` (host → all, optional): broadcasts the current lobby state (players + ready flags)

---

## 6. GUI – `tetris_gui` (wxWidgets)

### 6.1 `TetrisApp`
**Responsibility**
- Application entry point, initializes GUI framework and main window flow.

---

### 6.2 `StartFrame`
**Responsibility**
- First screen shown to user.
- Offers navigation to:
  - Single Player
  - Multiplayer
  - Exit (optional)

**Status note**
- Multiplayer lobby/ready screen is still under development.

---

### 6.3 `TetrisFrame`
**Responsibility**
- Single-player game window.
- Owns a `GameController` and displays a `GameState`.
- Maps keyboard input to `InputAction`.

---

### 6.4 `BoardPanel`
**Responsibility**
- Renders the current board grid and the active piece.

---

### 6.5 `NextPiecePanel`
**Responsibility**
- Renders the next tetromino preview.

---

### 6.6 `MultiplayerConfigDialog`
**Responsibility**
- Allows the player to configure multiplayer settings before connecting:
  - host/join selection
  - IP/port
  - match mode selection
  - mode parameters (time limit / pieces per turn)

---

### 6.7 `MultiplayerFrame`
**Responsibility**
- Main multiplayer view.
- Displays local/remote board(s) and match status.

**Planned lobby behavior**
- Show connected players.
- Provide "Ready" buttons for each local user.
- Start match only when all players are ready.

---

### 6.8 `RemoteBoardPanel`
**Responsibility**
- Renders a remote player's board state received from the network.

---

## 7. Planned / Missing Elements (Explicitly Tracked)

To remain aligned with the official IN204 multiplayer brief and the desired UX:

- **Lobby + Ready handshake UI**: not fully implemented yet.
- **Ready protocol**: confirm message type and host logic.
- **Mode extensibility**: already supported via `IMatchRules`, but UI and protocol must transmit selected mode and parameters reliably.
- **Robustness**: add validation for disconnects, timeouts, and malformed messages.

(Optionally, if required by the brief:)
- **Bot player**: a simulated client or local controller generating `InputAction`s.

---

## 8. Collaboration Summary

- Core provides the game mechanics and is reused by both single-player and multiplayer host logic.
- Controller centralizes time-based updates and action application.
- Networking implements host authority and state replication.
- GUI maps user intent to actions and visualizes state.
- Match rules are pluggable strategies, enabling new modes without architectural changes.