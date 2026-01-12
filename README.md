# Multiplayer Tetris – IN204 Project

A multiplayer-enabled implementation of **Tetris** written in **C++17**, developed for the **IN204 Object-Oriented Programming** course.

This repository contains:
- a complete **single-player Tetris engine**
- a **wxWidgets GUI** (main user interface)
- a **TCP-based multiplayer architecture** with two game modes (Time Attack, Shared Turns)

> Note: Multiplayer “lobby + ready” UX is part of the target design, but the lobby/ready screen and ready-handshake may still be under development depending on the current branch/state.

---

## 1) Project Description

### Single-player Tetris (implemented)
The project re-implements classic Tetris mechanics:
- Standard 7 Tetrominoes (I, O, T, L, J, S, Z)
- Lateral movement, rotations, gravity, line clearing
- Level progression and scoring (Tetris guideline-style)
- Pause/resume/reset and game over

### Multiplayer extension (implemented / evolving)
The multiplayer layer follows an **authoritative host** model:
- One player **hosts** a match
- Other players **join** remotely (different machines)
- Clients send **InputAction** requests to the host
- Host applies actions to authoritative state and broadcasts **StateUpdate** snapshots
- Match end results are shared via **MatchResult**

Multiplayer supports multiple modes (Strategy pattern):
- **Time Attack**: each player plays on their own board for a fixed duration; highest score wins
- **Shared Turns**: one shared board where players alternate control (by configurable pieces-per-turn)

---

## 2) Architecture Overview

The code is organized into evolutive modules:

### Core (Game Logic) – `tetris_core`
- Board representation, tetromino behavior, collisions, rotations
- Line clearing, scoring, level progression
- GameState lifecycle
- Match rules strategies (TimeAttack / SharedTurns)

### Controller
- `GameController` runs time-based gravity and applies input actions
- `InputAction` is UI/network agnostic

### GUI (wxWidgets) – `tetris_gui`
- `StartFrame` main menu
- `TetrisFrame` single-player gameplay view (board + next piece + HUD)
- Multiplayer configuration dialog and multiplayer views/panels

### Networking – `tetris_net`
- TCP sessions/server
- Message protocol + serialization
- Host/client orchestration
- State snapshot mapping for client rendering

This separation keeps the game engine independent from UI and networking, making the system maintainable and extensible.

---

## 3) Build & Run

### Requirements
- **C++17** or later
- **CMake ≥ 3.16**
- **vcpkg** (manifest mode supported via `vcpkg.json`)
- A compiler toolchain (MSVC / clang / gcc)

### Dependencies (vcpkg)
This project uses vcpkg manifest dependencies:
- `wxwidgets`

Ensure you have `VCPKG_ROOT` set (Windows example):
```powershell
setx VCPKG_ROOT "C:\vcpkg"
````

### Configure & Build

**Recommended (explicit toolchain):**

Windows (PowerShell):
```powershell
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build
````

Linux/macOS (bash):

```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

### Build options

* `BUILD_GUI` (default: ON) — build wxWidgets GUI executable
* `BUILD_TESTS` (default: ON) — build unit tests (Catch2 via FetchContent)

Example:

```bash
cmake -B build -S . -DBUILD_GUI=ON -DBUILD_TESTS=ON
cmake --build build
```

---

## 4) Running

After building, you will typically have two executables:

### GUI (main application)

Run:

* **`tetris_gui`**
  This is the main user-facing application (Start menu → Single Player / Multiplayer).

### Console (legacy/debug)

Run:

* **`tetris_console`**
  A minimal runner useful for debugging core logic without the GUI.

> The exact output path depends on your generator (e.g., `build/Debug/` on some IDE setups).

---

## 5) Multiplayer Usage (Current Intended Flow)

The multiplayer system is designed around:

1. Host creates a session and waits for a client to connect
2. Both players confirm **Ready**
3. Host sends `StartGame` and the match begins simultaneously
4. Clients send input actions; host broadcasts state updates

Currently implemented protocol messages include:

* `JoinRequest`, `JoinAccept`
* `StartGame` (includes mode + parameters + `startTick`)
* `InputActionMessage`
* `StateUpdate`
* `MatchResult`, `Error`

> Depending on the current implementation state, the lobby/ready UI and ready-handshake messages may still be pending.

---

## 6) Repository Structure

```
include/
  core/            # Board, GameState, Tetrominoes, scoring, levels, match rules
  controller/      # GameController, InputAction
  gui/             # wxWidgets UI (StartFrame, TetrisFrame, panels, dialogs)
  network/         # Protocol, serialization, host/client TCP networking

src/
  core/
  controller/
  gui/
  network/
  utils/
  main_console.cpp

docs/              # IN204 documentation (use cases, specs, architecture, UML)
tests/             # Unit tests
CMakeLists.txt
vcpkg.json
```

---

## 7) Documentation (IN204 Deliverables)

The project documentation targets the IN204 requirements:

* Use cases (single + multiplayer)
* Functional specifications
* High-level architecture (“gros grains”)
* Class responsibilities and design patterns
* UML diagrams (class + key sequences)
* Implementation notes (build/run, design justifications)

See the `docs/` folder.

---

## 8) Testing

Unit tests are built with Catch2 (via CMake FetchContent).

To run tests after building:

* Use `ctest` from the build directory:

```bash
cd build
ctest
```

---

## 9) Authors

Developed by:

* **Gustavo Loiola**
* **Felipe Biasuz**

Work distributed across:

* Core engine & controller
* GUI (wxWidgets)
* Networking & multiplayer logic

---

## 10) License

Educational use only (IN204 course project).