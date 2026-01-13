# Functional Specifications – Multiplayer Tetris (IN204)

This document lists the functional requirements implemented by the project and maps them to the current architecture and implementation state (core + networking + SDL/ImGui GUI).

## 1. Scope

The project implements:

1. A complete **single-player Tetris** game.
2. A **multiplayer extension** where players run the game on different machines and participate in the same match via TCP.

Multiplayer supports two modes:

* **Competitive Time Attack**
* **Shared Turns** (one shared board, alternating control by **pieces per turn**)

## 2. Terminology

* **Board / Well**: the grid where blocks fall and lines are cleared.
* **Tetromino**: one of the 7 Tetris pieces.
* **Lock**: when a falling piece becomes fixed in the board.
* **Host**: authoritative instance controlling the match state.
* **Client**: remote participant sending inputs and receiving state updates.
* **Snapshot**: a `StateUpdate` message containing a DTO view of the current state.

---

## 3. Single-player Requirements (Implemented)

### SP-01 – Board and collisions

The system shall maintain a rectangular grid board and prevent blocks from leaving bounds or overlapping fixed blocks.

### SP-02 – Tetromino set

The system shall provide the standard 7 tetrominoes: I, O, T, L, J, S, Z.

### SP-03 – Movement and rotation

The player shall be able to:

* move left/right when space permits,
* rotate pieces by 90° increments (CW/CCW),
* perform soft drop and hard drop.

### SP-04 – Gravity

The system shall apply gravity over time, making the active piece fall automatically.
Gravity speed depends on the current level.

### SP-05 – Line clearing

When a row becomes fully occupied, it shall be cleared and all above blocks shall fall.

### SP-06 – Scoring and level progression

The system shall update score and level based on cleared lines (course guideline policy).

### SP-07 – Game lifecycle

The system shall support:

* start/reset
* pause/resume
* game over when spawning is blocked / board overflows.

### SP-08 – Next piece preview

The UI shall display the next tetromino (single-player UI feature).

---

## 4. Multiplayer Requirements (Implemented)

### MP-01 – Remote play on different machines (Implemented)

The system shall allow two players to participate from different computers via TCP networking.

### MP-02 – Authoritative host (Implemented)

The system shall use a host-authoritative model:

* Clients send `InputActionMessage` to host.
* Host validates/applies inputs to authoritative state(s).
* Host broadcasts `StateUpdate` snapshots periodically (fixed rate).

### MP-03 – Join protocol (Implemented)

The system shall support a join handshake:

* Client sends `JoinRequest` including `playerName`.
* Host replies with `JoinAccept` including `assignedId`.

### MP-04 – Lobby behavior and start ownership (Implemented)

The system shall provide a multiplayer lobby:

* Host starts listening on a port and waits for a client connection.
* Client connects and joins with a player name.
* **Only the host can start the match.**

**UI constraint**

* When joining, the client UI must not allow changing match parameters; host parameters are authoritative.

### MP-05 – Host-selected configuration propagation (Implemented)

The match configuration shall be decided by the host and applied on clients:

* Host sends `StartGame` including:

  * `mode` (TimeAttack / SharedTurns)
  * mode parameters (`timeLimitSeconds` or `piecesPerTurn`)
  * `startTick`

Client updates its local `MultiplayerConfig` from the received `StartGame` before entering gameplay.

### MP-06 – State synchronization by snapshots (Implemented)

The host shall send `StateUpdate` messages sufficient for clients to render the match consistently.

`StateUpdate` includes at minimum:

* `serverTick`
* per-player `PlayerStateDTO`:

  * `id`, `name`
  * `BoardDTO` (grid snapshot)
  * `score`, `level`, `isAlive`

**Additional HUD fields (Implemented in current multiplayer)**
The snapshot also carries match HUD data used by the client UI:

* **TimeAttack**: time remaining (host authoritative, displayed on client)
* **SharedTurns**: current turn owner and pieces remaining in the turn (host authoritative)

> Note: These HUD fields are part of the current protocol evolution and are treated as required for correct multiplayer UX.

### MP-07 – End-of-match and results exchange (Implemented)

When a match ends, the system shall deliver a result to both players:

* Host decides outcome (Win/Lose/Draw).
* Host sends `MatchResult` to client.
* Both sides show an end-of-match overlay.

`MatchResult` includes:

* `endTick`
* `playerId`
* `outcome` (Win/Lose/Draw)
* `finalScore`

### MP-08 – Rematch agreement (Implemented)

After a match ends, the system shall allow a rematch:

* Both players must agree.
* The host restarts the match only if the opponent is still connected and has indicated readiness.

**Current mechanism**

* Client “requests rematch” by re-sending a `JoinRequest`.
* Host tracks readiness and, when both are ready, issues a new `StartGame` and resets authoritative state.

### MP-09 – Disconnect handling (Implemented)

The system shall handle mid-game disconnections gracefully:

* If the **client disconnects**, host:

  * detects disconnection
  * ends the match
  * shows “Opponent disconnected” in the result overlay
  * disables rematch if opponent is gone

* If the **host disconnects**, client:

  * detects missing snapshots via timeout
  * shows a “HOST DISCONNECTED” overlay
  * provides a button to return to the Start menu

### MP-10 – Error reporting (Implemented)

Networking/protocol errors can be communicated through `ErrorMessage` (kind `Error`) when applicable.

---

## 5. Multiplayer Modes (Match Rules)

### MP-MODE-01 – Competitive Time Attack (Implemented)

**Description**
Each player plays on their own board for a fixed duration. Winner is determined by:

* if only one player dies, the survivor wins
* otherwise higher score wins
* tie → draw

**Configuration**

* `StartGame.mode = TimeAttack`
* `StartGame.timeLimitSeconds > 0`
* `StartGame.piecesPerTurn = 0`

**UI**

* Displays two boards and two scoreboards (you and opponent)
* Displays **time remaining** (host authoritative)

---

### MP-MODE-02 – Shared Turns (Implemented)

**Description**
One shared board. Players alternate control based on **pieces per turn**. Only the current turn owner’s inputs are applied by the host.

**Configuration**

* `StartGame.mode = SharedTurns`
* `StartGame.piecesPerTurn > 0`
* `StartGame.timeLimitSeconds = 0`

**Rules**

* Host enforces turn ownership.
* Host broadcasts current turn state (turn player id + pieces remaining).

**UI**

* Displays one shared board + turn HUD.

---

## 6. User Interface Requirements (SDL2 + ImGui)

### UI-01 – Start menu (Implemented)

The first screen shall be a Start menu offering:

* Single Player
* Multiplayer
* Exit (optional)

### UI-02 – Multiplayer configuration (Implemented)

Before connecting, the UI shall allow:

* hosting or joining
* selecting match mode
* configuring mode parameters (time limit; pieces per turn)
* selecting IP/port (client)

### UI-03 – Multiplayer lobby (Implemented)

The lobby shall:

* show connection status
* show player name and assigned playerId (client)
* show server/client status (host)
* allow only host to start match
* enforce that client sees host-selected match parameters

### UI-04 – Multiplayer gameplay view (Implemented)

The multiplayer game view shall show:

* TimeAttack: both boards + scores + time remaining
* SharedTurns: shared board + current turn information
* end-of-match overlay with result and options (rematch/back)

### UI-05 – Disconnection UX (Implemented)

When disconnect occurs:

* client sees “HOST DISCONNECTED” overlay with button
* host sees “Opponent disconnected” in match result overlay

---

## 7. Non-Functional Requirements

### NFR-01 – Maintainability

Core engine remains independent of UI/networking and supports adding new modes via strategies/rules.

### NFR-02 – Robustness (multiplayer)

Networking handles:

* disconnect detection
* snapshot timeouts
* invalid messages (deserialize failure → error handling)

### NFR-03 – Portability

Project builds with CMake. Dependencies resolved via vcpkg manifest.

---

## 8. Traceability (Requirement → Module)

* SP-01..SP-07: `core/Board`, `core/Tetromino*`, `core/GameState`, `core/ScoreManager`, `core/LevelManager`
* SP-04, SP-07: `controller/GameController`
* MP-01..MP-03: `network/TcpServer`, `network/TcpSession`, `network/INetworkSession`, `network/NetworkHost`, `network/NetworkClient`, `network/Serialization`, `network/MessageTypes`
* MP-04..MP-05: `gui_sdl/LobbyScreen`, `network/NetworkHost`, `network/NetworkClient`
* MP-06: `network/StateUpdateMapper`, `network/MessageTypes`, `gui_sdl/MultiplayerGameScreen`
* MP-07: `gui_sdl/MultiplayerGameScreen` + host match logic
* MP-08: `gui_sdl/MultiplayerGameScreen` + host rematch readiness tracking
* MP-09: `gui_sdl/MultiplayerGameScreen` + host/client connection detection
* UI-* : `gui_sdl/Application`, `gui_sdl/Screen`, `gui_sdl/StartScreen`, `gui_sdl/MultiplayerConfigScreen`, `gui_sdl/LobbyScreen`, `gui_sdl/MultiplayerGameScreen`

