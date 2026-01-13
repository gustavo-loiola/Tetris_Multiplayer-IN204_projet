# Use Cases – Multiplayer Tetris (IN204)

This document describes the main use cases for the system in single-player and multiplayer contexts, aligned with the current implementation (SDL2 + ImGui GUI, TCP networking, authoritative host, snapshots, match results, rematch, disconnect handling).

## Actors

* **Player**: human user interacting with the game UI.
* **Host Player**: player who hosts a multiplayer match.
* **Client Player**: player who joins a hosted match.
* **System**: application and its subsystems (core/controller/network/UI).

---

## UC-01 – Start Application

**Primary Actor**: Player
**Goal**: Launch the application and reach the start menu.

**Preconditions**

* Application is installed/built.

**Main Flow**

1. Player launches the application.
2. System displays the **Start Screen** (`StartScreen`) with options (Single Player / Multiplayer).

**Postconditions**

* Player can select a mode.

---

## UC-02 – Play Single Player Game

**Primary Actor**: Player
**Goal**: Play a standard single-player Tetris session.

**Preconditions**

* Player is in the start menu.

**Main Flow**

1. Player selects **Single Player**.
2. System opens the gameplay screen.
3. System spawns tetrominoes and applies gravity.
4. Player moves/rotates/drops pieces via keyboard input.
5. System clears completed lines and updates score/level.
6. The game ends when spawning becomes impossible (game over).
7. Player may return to the menu.

**Alternative Flows**

* A1: Player pauses and resumes the game.

**Postconditions**

* A final score exists in the session state.

---

## UC-03 – Host Multiplayer Match (Lobby)

**Primary Actor**: Host Player
**Goal**: Create a multiplayer match configuration and wait for a client to join.

**Preconditions**

* Host Player is in start menu.
* Network is available.

**Main Flow**

1. Host Player selects **Multiplayer**.
2. System displays the multiplayer configuration screen.
3. Host Player configures:

   * Host mode (port)
   * Match mode (Time Attack / Shared Turns)
   * Mode parameters (**time limit** or **pieces per turn**)
4. Host confirms.
5. System starts listening (`TcpServer`) and opens the **Lobby Screen** (`LobbyScreen`).
6. When a client connects:

   1. Client sends `JoinRequest(playerName)`.
   2. Host replies with `JoinAccept(assignedId, welcomeMessage)`.
   3. Lobby displays updated connection/player count.
7. Host Player presses **Start Match**.
8. System sends `StartGame(mode, params, startTick)` to all clients.
9. System transitions host to `MultiplayerGameScreen` immediately.

**Postconditions**

* Match begins with host-selected parameters.

---

## UC-04 – Join Multiplayer Match (Lobby)

**Primary Actor**: Client Player
**Goal**: Join an existing multiplayer lobby and wait for the host to start.

**Preconditions**

* Client Player is in start menu.
* Knows host address and port.
* Network is available.

**Main Flow**

1. Client Player selects **Multiplayer**.
2. System displays multiplayer configuration screen.
3. Client Player chooses Join mode and enters:

   * host IP/hostname
   * port
   * player name
4. Client confirms.
5. System connects to host (`TcpSession` + `NetworkClient`).
6. Client sends `JoinRequest(playerName)`.
7. Host replies with `JoinAccept(assignedId, welcomeMessage)`.
8. System displays the **Lobby Screen**.
9. Client waits for `StartGame` from host.
10. On receiving `StartGame`, the client:

    1. Updates its local config with host parameters (mode/time limit/pieces per turn).
    2. Transitions to `MultiplayerGameScreen`.

**Postconditions**

* Client enters gameplay using host-selected parameters.

**Alternative Flows**

* A1: Host unreachable → show “Not connected” in lobby/config and allow back to menu.
* A2: Disconnect before start → lobby remains, user can return to menu.

---

## UC-05 – Start Multiplayer Match (Host Authority)

**Primary Actor**: Host Player
**Goal**: Start the match for everyone (host is the only one allowed).

**Preconditions**

* Host is in lobby.
* At least one client is connected.

**Main Flow**

1. Host presses **Start Match**.
2. System sends `StartGame` to clients.
3. Host enters gameplay; clients enter when they receive `StartGame`.
4. During gameplay:

   * Client sends `InputActionMessage`.
   * Host applies inputs to authoritative state(s).
   * Host broadcasts `StateUpdate` snapshots at a fixed rate.
5. Clients render using the received snapshots only (no client-side physics).

**Postconditions**

* A synchronized multiplayer match is running (authoritative host loop).

---

## UC-06 – Play Multiplayer Match (Time Attack)

**Primary Actor**: Host Player, Client Player
**Goal**: Compete for the highest score under a time limit.

**Preconditions**

* Match mode is **Time Attack**.

**Main Flow**

1. Host runs two authoritative game states:

   * host local board
   * opponent board (controlled by client inputs)
2. Client sends actions; host applies to opponent board.
3. Host sends `StateUpdate` containing both players’ DTOs (boards + scores + levels + alive flags) plus **time remaining**.
4. UI shows:

   * two boards
   * scoreboard for both players
   * timer with time remaining
5. Match ends when:

   * time runs out, or
   * one/both players reach game over.
6. Host computes winner and sends `MatchResult` to client.
7. Both host and client show a match result overlay.

**Postconditions**

* Match result is visible to all participants.

---

## UC-07 – Play Multiplayer Match (Shared Turns)

**Primary Actor**: Host Player, Client Player
**Goal**: Play on a single shared board where control alternates by **pieces per turn**.

**Preconditions**

* Match mode is **Shared Turns**.

**Main Flow**

1. Host runs one authoritative shared game state (`sharedGame_`).
2. Host selects the current turn owner (`turnPlayerId`) and initializes `piecesLeftThisTurn`.
3. Both players send input actions, but host applies only:

   * actions from the current turn owner, and
   * host local actions only when it is host’s turn.
4. After the configured number of pieces are committed/consumed, host switches turn owner.
5. Host broadcasts `StateUpdate` including:

   * shared board DTO
   * scoreboard info
   * current `turnPlayerId` and `piecesLeftThisTurn`
6. Client renders the shared board and turn HUD from snapshots.

**Postconditions**

* Shared board match finishes and results are shared.

---

## UC-08 – Match End Overlay + Rematch Agreement

**Primary Actor**: Host Player, Client Player
**Goal**: Offer clear end-of-match UX and allow a rematch only if both agree.

**Preconditions**

* A multiplayer match has ended (Time Attack or Shared Turns).

**Main Flow**

1. System displays a **Match Result** overlay on both screens.
2. Overlay shows:

   * outcome (Win/Lose/Draw)
   * final score
   * extra info (e.g., time limit for TimeAttack)
3. If both players choose **Rematch**:

   * Client requests rematch (current implementation: re-sends `JoinRequest` as a “ready” signal).
   * Host detects readiness and, if opponent is still connected, restarts the match:

     1. resets authoritative core state(s)
     2. broadcasts a new `StartGame`
4. Both screens resume gameplay.

**Alternative Flows**

* A1: One player returns to menu → rematch becomes impossible; host informs opponent is gone.
* A2: Opponent disconnected → overlay indicates rematch unavailable.

**Postconditions**

* Either a new match starts, or users return to the start menu.

---

## UC-09 – Handle Disconnect During Multiplayer (Lobby or Match)

**Primary Actor**: Host Player, Client Player
**Goal**: Keep system stable and inform users clearly when the network breaks.

**Preconditions**

* Multiplayer lobby or match is active.

### UC-09A – Host disconnects (client-side detection)

**Main Flow**

1. Host closes the game / returns to menu / connection drops.
2. Client stops receiving `StateUpdate`.
3. Client detects a snapshot timeout.
4. Client displays a **HOST DISCONNECTED** overlay with a button.
5. Client can return to Start menu.

**Postconditions**

* Client does not freeze; user gets an explicit recovery action.

### UC-09B – Client disconnects (host-side detection)

**Main Flow**

1. Client disconnects or closes the game.
2. Host detects disconnect via session state / poll.
3. Host ends the match and shows a result overlay indicating opponent disconnected.
4. Rematch is disabled.

**Postconditions**

* Host remains responsive; the state remains consistent.