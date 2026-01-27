# Architecture – Multiplayer Tetris (IN204)

This document describes the **high-level architecture** of the Multiplayer Tetris project.
The system is implemented in **modern C++ (C++17)** and follows a **modular, evolutive design** separating gameplay logic, controllers, networking, and user interface.

The project currently provides:

* a **SDL2 + Dear ImGui** graphical front-end,
* a **TCP-based, host-authoritative multiplayer model**,
* multiple multiplayer modes implemented via **match rule strategies**.

---

## 1. Architectural Goals

The architecture is designed to:

* Strictly separate **game rules** from **UI** and **networking**.
* Make the **core gameplay engine reusable** in different contexts (single-player, multiplayer, console, GUI).
* Support **multiple multiplayer modes** through interchangeable rule strategies.
* Use a **host-authoritative networking model** to ensure consistency and fairness.
* Isolate protocol and serialization concerns so the networking layer can evolve independently.
* Favor **maintainability, testability, and extensibility**.

---

## 2. Repository Modules (Macro-Architecture)

The project is decomposed into the following major modules:

```
tetris_core        → gameplay rules and state
controller         → time-based control and input application
tetris_net         → networking, protocol, and multiplayer orchestration
tetris_gui_sdl     → SDL2 + ImGui user interface
tetris_console     → minimal console-based runner (debug / legacy)
```

Dependencies follow a **strict direction**:

```
GUI / Networking
        ↓
   Controller
        ↓
      Core
```

The core module never depends on UI or networking.

---

## 3. Core Gameplay Module – `tetris_core`

### 3.1 Responsibilities

`tetris_core` contains **all gameplay rules and state**:

* Board representation and invariants.
* Tetromino geometry, movement, and rotation.
* Collision detection and line clearing.
* Scoring and level progression.
* Game lifecycle management (start, pause, game over).
* Multiplayer **match rule strategies** (mode-dependent logic).

This module is completely **independent of SDL, ImGui, and networking**.

---

### 3.2 Key Classes

**State and rules**

* `Board`
  Owns the grid of locked blocks, validates placement, clears full lines, and detects game-over conditions.

* `Tetromino` / `TetrominoFactory`
  Encapsulate tetromino shapes, rotations, and creation logic.

* `GameState`
  Represents a full Tetris session for one board:

  * board + active and next pieces,
  * scoring and level managers,
  * gameplay legality (movement, rotation, locking, clearing),
  * lifecycle state (`NotStarted`, `Running`, `Paused`, `GameOver`).

  `GameState` is an **active domain object**: it encapsulates both state and core gameplay rules.
  It does **not** manage time; that responsibility belongs to the controller layer.

* `ScoreManager`, `LevelManager`
  Encapsulate scoring rules and gravity/level progression independently from the board logic.

---

### 3.3 Multiplayer Match Rules (Strategy)

Multiplayer-specific policies are implemented using the **Strategy pattern**.

**Interface**

* `IMatchRules` (defined in `MatchRules.hpp`)

  This interface defines match-level rules that are **independent of per-cell gameplay mechanics**, such as:

  * when a match starts and ends,
  * how a winner is determined,
  * whose turn it is (for turn-based modes).

  Importantly, rules operate on **lightweight `PlayerSnapshot` DTOs**, not on full `GameState` objects.
  This avoids coupling match logic to board internals.

**Implementations**

* `TimeAttackRules`

  * Independent boards per player.
  * Match ends after a time limit or when players die.
  * Winner determined by survival and/or score.

* `SharedTurnRules`

  * One shared board.
  * Control alternates between players based on a fixed number of locked pieces per turn.
  * Match ends when only one (or zero) players remain alive.

This design allows adding new multiplayer modes without modifying the UI or networking layers.

---

## 4. Controller Layer – `controller/`

### 4.1 Responsibilities

The controller layer coordinates **time-based progression** and **player actions**:

* Converts discrete input events into gameplay actions.
* Applies gravity at intervals determined by the current level.
* Delegates all legality checks to `GameState`.

---

### 4.2 Key Classes

* `InputAction`
  Enumerates player intentions (MoveLeft, Rotate, SoftDrop, HardDrop, Pause, etc.).
  It is UI- and network-agnostic.

* `GameController`
  Owns the **time accumulator** and applies:

  * gravity ticks,
  * discrete input actions.

  The controller **does not own** the `GameState`; it operates on a reference provided by the caller.
  This makes the gameplay loop easy to test and reuse in different contexts.

---

## 5. Networking Module – `tetris_net`

### 5.1 Responsibilities

The networking module provides **online multiplayer support** using a **host-authoritative model**:

* Clients send input actions.
* The host simulates the authoritative game state(s).
* The host broadcasts periodic state snapshots.
* The host determines match results and lifecycle events.

All protocol, transport, and serialization logic is isolated in this module.

---

### 5.2 Transport and Sessions

* `INetworkSession`
  Abstract duplex message channel used by both host and client logic.

* `TcpSession`
  Concrete TCP implementation:

  * runs a background thread,
  * reads line-based messages,
  * deserializes messages and dispatches them via callbacks.

* `TcpServer`
  Accepts incoming TCP connections and creates `TcpSession` instances.

---

### 5.3 Host-Side Orchestration

Host-side multiplayer logic is explicitly split into three cooperating components:

* `NetworkHost`
  Manages connected clients, player identities, protocol handling, and input queues.
  It is responsible for:

  * join handshake,
  * input collection,
  * disconnect detection,
  * broadcasting messages.

* `HostGameSession`
  Coordinates the **match rules** (`IMatchRules`) with the networking layer:

  * initializes players,
  * starts the match,
  * forwards piece-lock events to rules,
  * evaluates match completion and results.

* `HostLoop`
  Acts as the **simulation glue**:

  * consumes client input messages,
  * dispatches actions to `GameController` instances,
  * advances gravity,
  * detects newly locked pieces,
  * periodically builds and broadcasts `StateUpdate` snapshots.

This separation keeps networking, rules, and simulation concerns loosely coupled.

---

### 5.4 Protocol and Serialization

* `MessageTypes`
  Defines all protocol messages (JoinRequest, StartGame, InputActionMessage, StateUpdate, MatchResult, etc.).

* `Serialization`
  Converts messages to/from **single-line UTF-8 text**.
  Each message is framed as one line to ensure reliable TCP parsing.

* `StateUpdateMapper`
  Converts `core::GameState` into DTO snapshots (`PlayerStateDTO`, `BoardDTO`) for network transmission.

The core engine never serializes itself; all mapping is done externally.

---

## 6. GUI Module – `tetris_gui_sdl`

### 6.1 Responsibilities

The SDL2 + ImGui GUI module:

* Provides all user interaction and rendering.
* Implements navigation between screens.
* Translates keyboard input into `InputAction`.
* Displays authoritative state (local or remote).

The GUI **never reimplements game rules**.

---

### 6.2 Screen Architecture

* `Application`
  Owns the SDL window, renderer, and ImGui context.
  Runs the main loop and manages screen transitions.

* `Screen`
  Abstract base class for UI states.

Concrete screens include:

* `StartScreen`
* `SinglePlayerScreen`
* `MultiplayerConfigScreen`
* `LobbyScreen`
* `MultiplayerGameScreen`

---

### 6.3 Multiplayer Rendering Model

* **Host**

  * Runs local simulation via `GameController`.
  * Applies remote inputs.
  * Broadcasts snapshots.

* **Client**

  * Does not simulate gameplay.
  * Sends input actions to host.
  * Renders the latest received `StateUpdate`.

This model favors **consistency and fairness** over client-side prediction.

---

## 7. Execution Flows (Summary)

### Single Player

```
Input → GameController → GameState → Render
```

### Multiplayer (Host)

```
Client Inputs → NetworkHost → HostLoop
             → GameController(s) → GameState(s)
             → StateUpdateMapper → Broadcast
```

### Multiplayer (Client)

```
Input → NetworkClient → Host
StateUpdate → Render only
```

---

## 8. Evolutivity Points

* **New multiplayer modes**: add new `IMatchRules` implementations.
* **New front-ends**: reuse core and networking with a different UI.
* **Protocol evolution**: extend message types or snapshots without touching core logic.

---

## 9. Design Patterns Used

* **Strategy**: multiplayer match rules (`IMatchRules`)
* **Factory**: tetromino creation (`TetrominoFactory`)
* **MVC-inspired layering**:

  * Model: `GameState`, `Board`, `Tetromino`
  * Controller: `GameController`
  * View: SDL/ImGui screens
* **RAII & Modern C++**:

  * smart pointers,
  * deterministic resource management,
  * STL containers
* **Host-authoritative networking**: standard multiplayer architecture pattern
