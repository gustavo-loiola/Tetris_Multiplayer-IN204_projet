# Functional Specifications – Multiplayer Tetris (IN204)

This document defines the **functional requirements** implemented by the Multiplayer Tetris project and maps them to the current implementation.
The specifications cover **single-player gameplay**, **multiplayer networking**, and the **SDL2 + ImGui user interface**.

---

## 1. Scope

The project implements:

1. A complete **single-player Tetris** game.
2. A **multiplayer extension** allowing players on different machines to participate in the same match via TCP networking.

The multiplayer implementation supports **two distinct game modes**:

* **Competitive Time Attack**
* **Shared Turns** (one shared board with turn-based control by number of pieces)

---

## 2. Terminology

* **Board / Well**: Rectangular grid where blocks fall and lines are cleared.
* **Tetromino**: One of the seven standard Tetris pieces.
* **Lock**: Event where the active tetromino becomes fixed in the board.
* **Host**: Authoritative instance that simulates the game and decides results.
* **Client**: Remote participant that sends inputs and renders snapshots.
* **Snapshot**: A `StateUpdate` message containing a DTO view of the authoritative state.
* **Tick**: Monotonic host-side counter used for match timing and synchronization.

---

## 3. Single-Player Functional Requirements

### SP-01 – Board and collisions 

The system shall maintain a rectangular grid board and prevent blocks from:

* moving outside board bounds,
* overlapping already locked blocks.

Collision rules are enforced exclusively by the core engine.

---

### SP-02 – Tetromino set 

The system shall support exactly the seven standard tetromino types:

* I, O, T, L, J, S, Z

Each tetromino consists of four blocks and supports four rotation states.

---

### SP-03 – Movement and rotation 

The player shall be able to:

* move the active tetromino left and right,
* rotate the tetromino clockwise or counter-clockwise,
* perform a soft drop (one cell down),
* perform a hard drop (drop until lock).

Rotations are validated against collisions and board bounds.

> Note: The rotation system does not implement advanced wall-kick (SRS) rules.

---

### SP-04 – Gravity and timing 

The system shall automatically move the active tetromino downward over time.

* Gravity speed depends on the current level.
* Gravity timing is managed by the controller layer, not by the game state.

---

### SP-05 – Line clearing 

When a row is fully occupied:

* the row shall be cleared,
* all rows above shall fall down accordingly.

The system supports clearing multiple rows at once.

---

### SP-06 – Scoring and level progression 

The system shall update:

* **score** based on the number of lines cleared and the current level,
* **level** based on cumulative cleared lines.

Additional small bonuses are awarded for soft and hard drops.

---

### SP-07 – Game lifecycle 

The system shall support:

* starting a game,
* pausing and resuming,
* resetting a session,
* detecting game over when new pieces cannot be spawned.

---

### SP-08 – Next piece preview 

The UI shall display the next tetromino to be spawned.

---

## 4. Multiplayer Functional Requirements

### MP-01 – Remote multiplayer 

The system shall allow two players to participate in the same match from different machines using TCP networking.

---

### MP-02 – Host-authoritative model 

The multiplayer system shall follow a **host-authoritative** model:

* Clients send `InputActionMessage` to the host.
* The host validates and applies inputs to authoritative game state(s).
* The host periodically broadcasts authoritative snapshots (`StateUpdate`).
* Clients do not simulate gameplay rules locally.

---

### MP-03 – Join protocol and identity assignment 

The system shall support a join handshake:

1. Client sends `JoinRequest(playerName)`.
2. Host assigns a unique `PlayerId` and replies with `JoinAccept`.

Player identifiers are used consistently throughout the match.

---

### MP-04 – Lobby and match ownership 

The system shall provide a lobby phase where:

* the host waits for client connections,
* players see connection status,
* **only the host can start the match**.

Clients cannot override host-selected match parameters.

---

### MP-05 – Match configuration propagation 

The host shall define match parameters, including:

* game mode,
* time limit (Time Attack),
* pieces per turn (Shared Turns).

The host shall send a `StartGame` message containing these parameters.
Clients must update their local configuration from this message before gameplay starts.

---

### MP-06 – Input synchronization 

During a match:

* clients send discrete input actions with a client-side tick,
* the host queues and applies inputs during simulation,
* the host enforces input validity (including turn ownership).

---

### MP-07 – State synchronization by snapshots 

The host shall periodically broadcast `StateUpdate` snapshots containing:

* server tick,
* per-player state (`PlayerStateDTO`):

  * board snapshot,
  * score,
  * level,
  * alive/dead status.

**Additional HUD fields included:**

* **Time Attack**: remaining time (host authoritative).
* **Shared Turns**: current turn owner and remaining pieces in the turn.

---

### MP-08 – Match end and result exchange 

When a match ends:

* the host determines the outcome,
* the host sends a `MatchResult` to all clients,
* both host and client display a result overlay.

Each `MatchResult` includes:

* end tick,
* player id,
* outcome (Win / Lose / Draw),
* final score.

---

### MP-09 – Rematch agreement 

After a match ends:

* both players may request a rematch,
* a new match starts only if **all connected players agree**,
* the host reinitializes authoritative state and sends a new `StartGame`.

---

### MP-10 – Disconnect handling 

The system shall handle network failures gracefully.

**Client disconnects:**

* Host detects disconnect.
* Host ends the match.
* UI displays “Opponent disconnected”.
* Rematch becomes unavailable.

**Host disconnects:**

* Client detects lack of messages (timeout).
* Client displays “HOST DISCONNECTED”.
* Client can return to the start menu.

---

### MP-11 – Liveness and keep-alive 

The host periodically sends `KeepAlive` messages to:

* prevent false-positive disconnect detection,
* maintain connection during lobby and post-match phases.

---

## 5. Multiplayer Modes

### MP-MODE-01 – Competitive Time Attack 

**Description**

Each player plays on an independent board for a fixed duration.

**End conditions**

* time limit reached, or
* one or both players reach game over.

**Winner determination**

* if only one player survives → survivor wins,
* otherwise highest score wins,
* equal scores → draw.

**UI**

* two boards,
* two scoreboards,
* host-authoritative time remaining.

---

### MP-MODE-02 – Shared Turns 

**Description**

Players share a single board and alternate control.

**Rules**

* Only the current turn owner’s inputs are applied.
* Turn switches after a fixed number of locked pieces.
* Host enforces turn ownership and broadcasts turn state.

**UI**

* shared board,
* turn owner indicator,
* remaining pieces in the current turn.

---

## 6. User Interface Requirements (SDL2 + ImGui)

### UI-01 – Start menu 

The system shall provide a start menu allowing the player to:

* start a single-player game,
* configure and enter multiplayer,
* exit the application.

---

### UI-02 – Multiplayer configuration 

The UI shall allow:

* selecting host or join role,
* entering player name,
* selecting game mode and parameters,
* entering network address and port (client).

---

### UI-03 – Multiplayer lobby 

The lobby shall:

* display connection status,
* display player identities,
* allow only the host to start the match,
* prevent clients from modifying match parameters.

---

### UI-04 – Multiplayer gameplay view 

The gameplay view shall render:

* authoritative board(s),
* score and level information,
* mode-specific HUD elements,
* match result overlays.

---

### UI-05 – Disconnection UX 

On disconnection:

* the user is explicitly informed,
* recovery actions (return to menu) are provided,
* the application remains responsive.

---

## 7. Design Decisions and Limitations

The following design choices are intentional:

* No advanced rotation wall-kicks (SRS).
* No T-Spin detection or scoring.
* Snapshot-based synchronization (no client-side prediction).
* Focus on two-player multiplayer (architecture extensible to more).

These decisions favor **clarity, robustness, and architectural simplicity**.

---

## 8. Traceability (Requirement → Module)

* **SP-01 – SP-08**
  `core/Board`, `core/Tetromino*`, `core/GameState`, `core/ScoreManager`, `core/LevelManager`

* **SP-04, SP-07**
  `controller/GameController`

* **MP-01 – MP-11**
  `network/NetworkHost`, `network/NetworkClient`, `network/HostGameSession`,
  `network/HostLoop`, `network/TcpSession`, `network/Serialization`, `network/MessageTypes`

* **UI-01 – UI-05**
  `gui_sdl/Application`, `gui_sdl/Screen`, `gui_sdl/*Screen`
