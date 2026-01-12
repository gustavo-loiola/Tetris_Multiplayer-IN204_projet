# Use Cases – Multiplayer Tetris (IN204)

This document describes the main use cases for the system in single-player and multiplayer contexts.

## Actors

- **Player**: a human user interacting with the game UI.
- **Host Player**: a player who hosts a multiplayer match.
- **Client Player**: a player who joins a hosted match.
- **System**: the application and its subsystems (core/controller/network/UI).

---

## UC-01 – Start Application

**Primary Actor**: Player  
**Goal**: Launch the application and reach the start menu.

**Preconditions**
- Application is installed/built.

**Main Flow**
1. Player launches the application.
2. System displays `StartFrame` with options (Single Player / Multiplayer / Exit).

**Postconditions**
- Player can select a mode.

---

## UC-02 – Play Single Player Game

**Primary Actor**: Player  
**Goal**: Play a standard single-player Tetris session.

**Preconditions**
- Player is in the start menu.

**Main Flow**
1. Player selects **Single Player**.
2. System opens the gameplay window (`TetrisFrame`).
3. System spawns tetrominoes and applies gravity.
4. Player moves/rotates/drops pieces using keyboard input.
5. System clears completed lines and updates score/level.
6. The game ends when the board blocks spawning (game over).
7. Player may restart or exit back to menu (if supported).

**Alternative Flows**
- A1: Player pauses and resumes the game.

**Postconditions**
- Final score is shown or stored in session state.

---

## UC-03 – Host Multiplayer Match (Lobby)

**Primary Actor**: Host Player  
**Goal**: Create a multiplayer match and wait for others to join.

**Preconditions**
- Host Player is in start menu.
- Network is available.

**Main Flow**
1. Host Player selects **Multiplayer**.
2. System displays multiplayer configuration dialog.
3. Host Player configures:
   - Host mode (port)
   - Match mode (Time Attack / Shared Turns)
   - Mode parameters (time limit / pieces per turn)
4. Host confirms.
5. System starts listening (`TcpServer`) and enters **Lobby** state.
6. When a client connects:
   1. Client sends `JoinRequest(playerName)`.
   2. Host replies with `JoinAccept(assignedId, welcomeMessage)`.
7. System shows the lobby participants and readiness status (UI planned).

**Postconditions**
- Lobby exists and players may proceed to ready up and start.

---

## UC-04 – Join Multiplayer Match

**Primary Actor**: Client Player  
**Goal**: Join an existing multiplayer match hosted on another machine.

**Preconditions**
- Client Player is in start menu.
- Knows host address and port.
- Network is available.

**Main Flow**
1. Client Player selects **Multiplayer**.
2. System displays multiplayer configuration dialog.
3. Client Player chooses Join mode (host IP/port) and confirms.
4. System connects to host (`TcpSession` via `NetworkClient`).
5. Client sends `JoinRequest(playerName)`.
6. Host replies with `JoinAccept(assignedId, welcomeMessage)`.
7. Client enters Lobby state and waits for match start.

**Alternative Flows**
- A1: Host unreachable → show error and allow retry.
- A2: Join rejected → show reason and return to menu/config.

**Postconditions**
- Client is registered and can ready up.

---

## UC-05 – Ready Up and Start Multiplayer Match (Synchronized Start)

**Primary Actor**: Host Player (start), Host+Client Players (ready)  
**Goal**: Ensure both players explicitly confirm readiness before the match begins.

**Preconditions**
- Host has created a lobby.
- At least one client joined and received `JoinAccept`.

**Main Flow**
1. Both Host Player and Client Player click **Ready** in the lobby UI (planned screen).
2. (Planned protocol) Each client sends `ReadyUp(playerId, ready=true)` to host.
3. Host tracks readiness for all participants.
4. When readiness condition is satisfied (e.g., all players ready):
   1. Host chooses a `startTick` slightly in the future.
   2. Host sends `StartGame(mode, params, startTick)` to all clients.
5. All players begin gameplay at the same time.
6. During gameplay:
   - Clients send `InputActionMessage`.
   - Host applies inputs authoritatively and broadcasts `StateUpdate`.

**Postconditions**
- Multiplayer match is running and synchronized.

---

## UC-06 – Play Multiplayer Match (Time Attack)

**Primary Actor**: Host Player, Client Players  
**Goal**: Compete for the highest score under a time limit.

**Preconditions**
- Match mode is Time Attack.

**Main Flow**
1. Each player plays on their own board.
2. Match runs until configured duration expires.
3. Host computes results and determines winner/ranking.
4. Host sends results to clients.

**Postconditions**
- Match results are visible to all participants.

---

## UC-07 – Play Multiplayer Match (Shared Turns)

**Primary Actor**: Host Player, Client Players  
**Goal**: Play on a single shared board where control alternates.

**Preconditions**
- Match mode is Shared Turns.

**Main Flow**
1. All players see the same shared board state.
2. System assigns an active player for the current turn (or N pieces).
3. Only active player inputs are accepted by host.
4. After turn condition is met, system switches active player.
5. Match ends based on game over condition or match policy.

**Postconditions**
- Shared board match finishes and results are shared.

---

## UC-08 – Handle Network Failure During Multiplayer

**Primary Actor**: Host Player, Client Player  
**Goal**: Keep system stable and inform the user if network issues occur.

**Preconditions**
- Multiplayer match is running or in lobby.

**Main Flow**
1. A disconnect or timeout occurs.
2. System detects socket failure.
3. System notifies affected players.
4. System applies a policy:
   - remove the disconnected player
   - or end the match
   - or allow reconnection (optional)

**Postconditions**
- System remains stable; match state is consistent with applied policy.
