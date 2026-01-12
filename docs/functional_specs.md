# Functional Specifications – Multiplayer Tetris (IN204)

This document lists the functional requirements implemented by the project and maps them to the current architecture.

## 1. Scope

The project implements:

1. A complete **single-player Tetris** game.
2. A **multiplayer extension** where players run the game on different machines and participate in the same match.

Multiplayer supports at least two modes:
- Competitive Time Attack
- Shared Turns (one shared board, alternating control)

## 2. Terminology

- **Board / Well**: the grid where blocks fall and lines are cleared.
- **Tetromino**: one of the 7 Tetris pieces.
- **Lock**: when a falling piece becomes fixed in the board.
- **Host**: authoritative instance controlling the match state.
- **Client**: remote participant sending inputs and receiving state updates.

## 3. Single-player Requirements (Implemented)

### SP-01 – Board and collisions
The system shall maintain a rectangular grid board and prevent blocks from leaving bounds or overlapping fixed blocks.

### SP-02 – Tetromino set
The system shall provide the standard 7 tetrominoes: I, O, T, L, J, S, Z.

### SP-03 – Movement and rotation
The player shall be able to:
- move left/right when space permits,
- rotate pieces by 90° increments,
- perform soft and hard drop (if implemented in controller input map).

### SP-04 – Gravity
The system shall apply gravity over time, making the active piece fall automatically.
Gravity speed shall depend on the current level.

### SP-05 – Line clearing
When a row becomes fully occupied, it shall be cleared and all above blocks shall fall.

### SP-06 – Scoring and level progression
The system shall update score and level based on cleared lines according to the course guideline.

### SP-07 – Game lifecycle
The system shall support:
- start/reset
- pause/resume
- game over when spawning is blocked / board overflows.

### SP-08 – Next piece preview
The UI shall display the next tetromino.

---
## 4. Multiplayer Requirements (Implemented / Planned)

### MP-01 – Remote play on different machines (Implemented)
The system shall allow multiple players to participate from different computers via TCP networking.

### MP-02 – Authoritative host (Implemented)
The system shall use a host-authoritative model:
- Clients send `InputActionMessage` to host.
- Host validates and applies inputs to authoritative state(s).
- Host broadcasts `StateUpdate` snapshots.

### MP-03 – Join protocol (Implemented)
The system shall support a join handshake:
- Client sends `JoinRequest` including `playerName`.
- Host replies with `JoinAccept` including `assignedId`.

### MP-04 – Lobby and readiness handshake (Planned)
After successful join, the system shall enter a lobby phase:
- Host waits for at least one client connection.
- Each player must explicitly indicate they are ready.
- Match starts only when all required participants are ready.

**Protocol impact (planned extension)**
- Introduce a `ReadyUp` message (client → host) and optionally a `LobbyUpdate` message (host → all).

### MP-05 – Synchronized start (Implemented / Planned)
The match shall start simultaneously for all players:
- Host sends `StartGame` containing:
  - `mode` (TimeAttack / SharedTurns)
  - mode parameters (`timeLimitSeconds` or `piecesPerTurn`)
  - `startTick` (host tick when the match begins)

### MP-06 – State synchronization (Implemented)
The host shall send `StateUpdate` messages sufficient for clients to render the match consistently.
`StateUpdate` includes:
- `serverTick`
- per-player `PlayerStateDTO` containing board snapshot, score, level, and alive state.

### MP-07 – Game results exchange (Implemented)
When a match ends, the system shall send `MatchResult` including:
- `endTick`
- `playerId`
- `outcome` (Win/Lose/Draw)
- `finalScore`

### MP-08 – Error reporting (Implemented)
Networking and protocol errors shall be communicated via `ErrorMessage` (kind `Error`) when applicable.

---

## 5. Multiplayer Modes (Match Rules Strategy)

### MP-MODE-01 – Competitive Time Attack (Implemented)
**Description**: Each player plays on their own board for a fixed duration. Highest score wins.

**Protocol configuration**
- `StartGame.mode = TimeAttack`
- `StartGame.timeLimitSeconds` set by host UI
- `StartGame.piecesPerTurn = 0`

### MP-MODE-02 – Shared Turns (Implemented)
**Description**: One shared board, players alternate controlling pieces according to configuration.

**Protocol configuration**
- `StartGame.mode = SharedTurns`
- `StartGame.piecesPerTurn` set by host UI
- `StartGame.timeLimitSeconds = 0`

---

## 6. User Interface Requirements (wxWidgets)

### UI-01 – Start menu
The first screen shall be a Start menu offering:
- Single Player
- Multiplayer
- Exit (optional)

### UI-02 – Single-player gameplay view
The game view shall show:
- board rendering
- next piece preview
- score / level / status

### UI-03 – Multiplayer configuration
Before connecting, the UI shall allow:
- hosting or joining
- selecting match mode
- configuring mode parameters (time limit; pieces per turn)

### UI-04 – Multiplayer gameplay view
The multiplayer view shall show:
- local player board (when applicable)
- remote board(s) or shared board state
- match status (time remaining / current turn)

---

## 7. Non-Functional Requirements

### NFR-01 – Maintainability
Core engine must remain independent of UI/networking and support addition of new modes.

### NFR-02 – Robustness (multiplayer)
Networking layer should handle:
- disconnects
- timeouts
- malformed or partial messages (TCP framing)

### NFR-03 – Portability
Project must build with CMake. Dependencies are resolved via vcpkg manifest.

---

## 8. Traceability (Requirement → Module)

- SP-01..SP-06: `core/Board`, `core/Tetromino*`, `core/GameState`, `core/ScoreManager`, `core/LevelManager`
- SP-04, SP-07: `controller/GameController`, `gui/TetrisFrame`
- MP-01..MP-05: `network/*` (HostLoop, HostGameSession, TcpServer/TcpSession, Serialization, StateUpdateMapper)
- MP-MODE-01..02: `core/*Rules` via `IMatchRules`
- UI-* : `gui/StartFrame`, `gui/TetrisFrame`, `gui/Multiplayer*`, `gui/RemoteBoardPanel`
