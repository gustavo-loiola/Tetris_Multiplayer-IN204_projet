# Multiplayer Tetris – IN204 Project

A multiplayer-enabled implementation of **Tetris** written in **C++17**, developed for the **IN204 Object-Oriented Programming** course.

This repository contains:
- a complete **single-player Tetris engine**
- a **SDL2 + Dear ImGui GUI** (main user interface)
- a **TCP-based multiplayer architecture** with two game modes (**Time Attack**, **Shared Turns**)
- a **host-authoritative** multiplayer model (clients send inputs, host simulates and broadcasts snapshots)

---

## 1) Project Description

### Single-player Tetris (implemented)
Classic Tetris mechanics:
- Standard 7 Tetrominoes (I, O, T, L, J, S, Z)
- Lateral movement, rotation, gravity, line clearing
- Level progression and scoring
- Pause/resume and game over

### Multiplayer (implemented)
The multiplayer layer follows an **authoritative host** model:
- One player **hosts** a match (server)
- Another player **joins** remotely (client)
- Client sends **InputAction** messages to host
- Host applies actions to authoritative `GameState`(s)
- Host broadcasts **StateUpdate** snapshots (periodic, ~20 Hz) for client rendering
- Match end results are shared via **MatchResult**
- Disconnects are detected and displayed (e.g., **HOST DISCONNECTED**, **Opponent disconnected**)

**Implemented modes:**
- **Time Attack**: each player plays on their own board; match ends on time limit or game over; winner by survival/score.
- **Shared Turns**: one shared board; control alternates **by pieces-per-turn** (only the active player inputs are accepted by host).

---

## 2) Architecture Overview

The code is organized into evolutive modules:

### Core (Game Logic) – `tetris_core`
- Board representation, tetromino behavior, collisions, rotation
- Line clearing, scoring, level progression
- `GameState` lifecycle and legality of moves
- Match rules strategies (TimeAttack / SharedTurns)

### Controller – `controller/`
- `GameController` runs gravity / timing and applies `InputAction`
- `InputAction` is UI- and network-agnostic

### Networking – `tetris_net`
- TCP transport (`TcpServer`, `TcpSession`)
- Protocol (`MessageTypes`) + text serialization (`Serialization`)
- Host orchestration (`NetworkHost`) and client interface (`NetworkClient`)
- Snapshot mapping between core state and DTOs (`StateUpdateMapper`)

### GUI (SDL2 + ImGui) – `tetris_gui_sdl`
- Screen state machine (`Screen`, `Application`)
- Screens:
  - `StartScreen`
  - `MultiplayerConfigScreen` (host/join + params)
  - `LobbyScreen` (connection + start match for host)
  - `MultiplayerGameScreen` (boards + scoreboards + match overlay + rematch/disconnect UX)

**Important design rule:**  
`tetris_core` does **not** depend on GUI or networking. UI/network depend on core.

---

## 3) Build & Run

### Requirements
- **C++17** or later
- **CMake ≥ 3.16**
- A compiler toolchain (MSVC / clang / gcc)

### Configure & Build (Windows PowerShell)
```powershell
cmake -B build -S .
cmake --build build --target tetris_gui_sdl
````

### Configure & Build (Linux/macOS bash)

```bash
cmake -B build -S .
cmake --build build --target tetris_gui_sdl
````

> Output path depends on generator (e.g. `build/Debug/` with MSVC).

---

## 4) Running

### GUI (main application)

Run the SDL executable:

* **`tetris_gui_sdl`**

It includes:

* Start menu
* Multiplayer configuration + lobby
* Multiplayer match screen (TimeAttack / SharedTurns)

### Console (optional / debug)

* **`tetris_console`** (useful to debug core logic without GUI)

---

## 5) Multiplayer Usage

### Host

1. Open **Multiplayer**
2. Choose **Host**, pick mode + parameters (time limit or pieces-per-turn), choose a port
3. Wait for a client to join (Lobby shows connection status)
4. Click **Start Match**
5. Host simulates and broadcasts snapshots + results

### Client

1. Open **Multiplayer**
2. Choose **Join**, enter host IP + port, choose your name
3. Client connects and waits in Lobby
4. When host starts, client enters match screen automatically
5. Client sends inputs, renders host snapshots

### Rematch

After match end:

* Both players can request **Rematch**
* Host restarts the match only when both agree
* If someone leaves, rematch becomes unavailable and UI shows the disconnect state

---

## 6) Protocol (Current)

Core messages used by the multiplayer system:

* `JoinRequest`, `JoinAccept`
* `StartGame`
* `InputActionMessage`
* `StateUpdate` (snapshot DTO + HUD fields like time left / turn state as implemented)
* `MatchResult`
* `Error`
* `PlayerLeft`

Serialization is **one message per line** of UTF-8 text.

---

## 7) Repository Structure

```
include/
  core/            # Board, GameState, Tetrominoes, scoring, levels, match rules
  controller/      # GameController, InputAction
  gui_sdl/         # SDL2 + ImGui screens
  network/         # Protocol, serialization, host/client networking

src/
  core/
  controller/
  gui_sdl/
  network/
  utils/
  main_console.cpp

docs/              # IN204 documentation (use cases, specs, architecture, UML)
tests/             # Unit tests
CMakeLists.txt
```

---

## 8) Documentation (IN204 Deliverables)

The `docs/` folder contains:

* Use cases
* Functional specifications
* High-level architecture
* Class responsibilities + design patterns
* UML diagrams (class + key sequences)

---

## 9) Authors

Developed by:

* **Gustavo Loiola**
* **Felipe Biasuz**

---

## 10) License

Educational use only (IN204 course project).

