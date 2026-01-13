# Multiplayer Tetris – IN204 Project

A multiplayer-enabled implementation of **Tetris** written in **C++**, completed as part of the **IN204 Object-Oriented Programming** course.
The project includes a full single-player Tetris engine and extends it with online multiplayer where each player participates from a different machine, as required in the official project brief.

---

## 1. Project Description

This software re-implements the classic Tetris rules:

* Standard grid (“well”) and the 7 Tetrominoes
* Lateral movement, rotations, gravity, and line clearing
* Level progression and scoring rules according to the guideline 

A second layer adds **multiplayer gameplay**:

* One player may **host** a match
* Other players can **join** remotely
* All participants start simultaneously
* Game results (score, game over) are exchanged through a lightweight protocol

The architecture follows OOP principles, modular decomposition, and UML-driven modeling as expected in IN204.

---

## 2. Architecture Overview

The project is structured around evolutive modules:

### **Core (Game Logic)**

Implements board representation, Tetromino behavior, collisions, rotations, line clearing, scoring, and level updates.

### **Controller**

Coordinates the game loop, user inputs, fall timing, and application of rules.

### **User Interface**

Graphical display built using **GTK** or **wxWidgets**: rendering of the board, score HUD, menus, and multiplayer lobby.

### **Networking**

Client–host communication layer supporting invitations and synchronized start. Handles Join, Start, State Update, and Game Over messages.

This separation allows the game engine to remain independent from the UI and networking code, making the system extensible and maintainable.

---

## 3. Build & Run

### **Requirements**

* C++17 or later
* CMake ≥ 3.10
* [vcpkg](https://github.com/microsoft/vcpkg) installed
* vcpkg install wxwidgets:x64-mingw


### **Setting up vcpkg**

1. Clone and bootstrap vcpkg (only once):

   ```powershell
   git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
   cd C:\vcpkg
   .\bootstrap-vcpkg.bat

### **Build**

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

```bash
mkdir build
cd build
cmake -G "MinGW Makefiles" ..
```

### **Run – Single Player**

```bash
./Debug/tetris_console.exe
```

### **Run – Multiplayer**

Host:

```bash
./tetris --host --port 4242
```

Join:

```bash
./tetris --join <host-ip> --port 4242
```

---

## 4. Repository Structure

```
src/
  core/          # Board, Tetromino, game rules
  controller/    # GameController
  gui/           # User interface
  network/       # Multiplayer (host & client)
include/         # Public headers
docs/            # Analysis, architecture, UML
tests/           # Unit tests
CMakeLists.txt
```

---

## 5. Documentation

This repository includes all required IN204 documentation:

* **Use-case analysis** (solo & multiplayer scenarios)
* **Functional analysis**
* **High-level architectural description** (“architecture gros grains”)
* **UML diagrams** (use cases, class diagrams, key sequence diagrams)
* **Implementation documentation**, including justification of class design and design patterns

These correspond to the deliverables requested in the project specification.

---

## 6. Authors

Developed by **Gustavo Loiola** and **Felipe Biasuz** with distributed responsibilities across:

* Core game engine
* Graphical interface
* Networking and multiplayer logic

---

## 7. License

This project is provided for educational use within the IN204 course.
