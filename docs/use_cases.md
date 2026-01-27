# Use Cases – Multiplayer Tetris (IN204)

This document describes the main use cases for the system in single-player and multiplayer contexts, aligned with the current implementation (**SDL2 + ImGui GUI**, **TCP networking**, **authoritative host**, **snapshots**, **match results**, **rematch**, **disconnect handling**, **keep-alive**).

## Actors

* **Player**: human user interacting with the game UI.
* **Host Player**: player who hosts a multiplayer match (authoritative).
* **Client Player**: player who joins a hosted match (remote).
* **System**: application and its subsystems (core/controller/network/UI).

---

## UC-01 – Start Application

**Primary Actor**: Player
**Goal**: Launch the application and reach the start menu.

**Preconditions**

* Application is built and runnable.

**Main Flow**

1. Player launches the application.
2. System initializes SDL and ImGui (`Application`).
3. System displays the **Start Screen** (`StartScreen`) with options (Single Player / Multiplayer).

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
2. System opens the **SinglePlayerScreen**.
3. System creates a local `GameState` and a `GameController`.
4. System spawns tetrominoes and applies gravity through the controller update loop.
5. Player moves/rotates/drops pieces via keyboard input mapped to `InputAction`.
6. System clears completed lines and updates score/level.
7. The game ends when spawning becomes impossible (game over).
8. Player may return to the start menu.

**Alternative Flows**

* A1: Player pauses and resumes the game (`PauseResume`).

**Postconditions**

* A final score exists in the session state.

---

## UC-03 – Configure Multiplayer Match Parameters

**Primary Actor**: Player
**Goal**: Choose the multiplayer role and parameters before connecting.

**Preconditions**

* Player is in the start menu.

**Main Flow**

1. Player selects **Multiplayer**.
2. System displays **MultiplayerConfigScreen**.
3. Player selects role:

   * Host, or
   * Join
4. Player enters:

   * player name
   * host address (Join only)
   * port
5. Player selects match mode:

   * Time Attack (time limit), or
   * Shared Turns (pieces per turn)
6. Player confirms configuration.

**Postconditions**

* System transitions to `LobbyScreen` with the selected configuration.

---

## UC-04 – Host Multiplayer Match (Lobby)

**Primary Actor**: Host Player
**Goal**: Create a multiplayer match configuration and wait for a client to join.

**Preconditions**

* Host Player has configured multiplayer as Host.
* Network is available.

**Main Flow**

1. Host Player enters the **Lobby Screen** (`LobbyScreen`).
2. System starts listening on a TCP port (`TcpServer`).
3. System creates `NetworkHost`.
4. When a client connects:

   1. Client sends `JoinRequest(playerName)`.
   2. Host assigns `PlayerId` and replies with `JoinAccept(assignedId, welcomeMessage)`.
   3. Lobby displays updated connection/player status.
5. Host may wait for the client or proceed.

**Postconditions**

* At least one client is connected and has been assigned a `PlayerId`.

**Alternative Flows**

* A1: Client disconnects before match start → lobby updates and host can continue waiting.

---

## UC-05 – Join Multiplayer Match (Lobby)

**Primary Actor**: Client Player
**Goal**: Join an existing multiplayer lobby and wait for the host to start.

**Preconditions**

* Client Player has configured multiplayer as Join.
* Knows host address and port.
* Network is available.

**Main Flow**

1. Client enters the **Lobby Screen**.
2. System connects to the host (`TcpSession::createClient`).
3. System creates `NetworkClient(session, playerName)`.
4. Client sends `JoinRequest(playerName)`.
5. Host replies with `JoinAccept(assignedId, welcomeMessage)`.
6. Client displays lobby status and waits for match start.
7. Host periodically sends `KeepAlive` messages while waiting (liveness support).
8. Client remains connected and responsive in the lobby.

**Postconditions**

* Client is connected and waiting for `StartGame`.

**Alternative Flows**

* A1: Host unreachable → UI indicates failure and offers return to menu.
* A2: Disconnect before start → UI indicates disconnect and offers return to menu.

---

## UC-06 – Start Multiplayer Match (Host Authority)

**Primary Actor**: Host Player
**Goal**: Start the match for all connected players (host is the only one allowed).

**Preconditions**

* Host is in lobby.
* At least one client is connected and joined.

**Main Flow**

1. Host clicks **Start Match**.
2. System sends `StartGame(mode, timeLimitSeconds, piecesPerTurn, startTick)` to all clients.
3. Host transitions to `MultiplayerGameScreen` immediately.
4. Clients transition to `MultiplayerGameScreen` when receiving `StartGame`.
5. Gameplay begins under host authority.

**Postconditions**

* A synchronized multiplayer match is running.

---

## UC-07 – Play Multiplayer Match (Time Attack)

**Primary Actor**: Host Player, Client Player
**Goal**: Compete for the highest score under a time limit.

**Preconditions**

* Match mode is **Time Attack**.
* `StartGame.timeLimitSeconds > 0`.

**Main Flow**

1. Host runs authoritative simulation for:

   * host board (local input),
   * client board (remote inputs).
2. Client sends `InputActionMessage(playerId, clientTick, action)` to the host.
3. Host applies client actions to the client’s authoritative `GameState` via `GameController`.
4. Host periodically broadcasts `StateUpdate` including:

   * `serverTick`,
   * `players[]` boards + score + level + alive,
   * `timeLeftMs` (host authoritative).
5. Client renders using the most recent `StateUpdate` snapshot (no client physics).
6. Match ends when:

   * time runs out, or
   * one/both players reach game over.
7. Host computes outcome and sends `MatchResult` to client.
8. Both sides display a match overlay with results.

**Postconditions**

* Match results are displayed to all participants.

---

## UC-08 – Play Multiplayer Match (Shared Turns)

**Primary Actor**: Host Player, Client Player
**Goal**: Play on a single shared board where control alternates by **pieces per turn**.

**Preconditions**

* Match mode is **Shared Turns**.
* `StartGame.piecesPerTurn >= 1`.

**Main Flow**

1. Host runs one authoritative shared `GameState`.
2. Host maintains current turn owner and piece counter (rule-based).
3. Both players may send input actions, but host applies only actions from:

   * the current turn owner, and
   * host local input when host is the current player.
4. When a piece locks:

   * host detects it (based on `GameState::lockedPieces()` changes),
   * host informs match rules (`onPieceLocked`),
   * rules may switch turn owner.
5. Host broadcasts `StateUpdate` including:

   * shared board DTO (in `players[]`),
   * turn HUD fields: `turnPlayerId`, `piecesLeftThisTurn`.
6. Client renders shared board + turn HUD from snapshots.
7. Match ends when rules determine completion (e.g., survival condition).
8. Host sends `MatchResult` to client.
9. Both display match overlay.

**Postconditions**

* Shared board match finishes and results are visible.

---

## UC-09 – Match End Overlay + Rematch Agreement

**Primary Actor**: Host Player, Client Player
**Goal**: Offer clear end-of-match UX and allow a rematch only if both agree.

**Preconditions**

* A multiplayer match has ended and `MatchResult` has been delivered.

**Main Flow**

1. System displays a **Match Result** overlay on both host and client.
2. Overlay shows:

   * outcome (Win/Lose/Draw),
   * final score,
   * mode-relevant context (e.g., time limit).
3. Each player chooses:

   * **Rematch**, or
   * **Back to Menu**.
4. If a player chooses rematch, the system sends `RematchDecision{wantsRematch=true}`.
5. If a player declines, the system sends `RematchDecision{wantsRematch=false}`.
6. Host restarts a match only when **all connected players** want rematch:

   1. host resets authoritative state(s),
   2. host sends a new `StartGame`.
7. Both sides resume gameplay.

**Alternative Flows**

* A1: One player returns to menu → rematch becomes impossible.
* A2: Opponent disconnected → overlay indicates rematch unavailable.

**Postconditions**

* Either a new match starts, or users return to the start menu.

---

## UC-10 – Handle Disconnect During Multiplayer (Lobby or Match)

**Primary Actor**: Host Player, Client Player
**Goal**: Keep the system stable and inform users clearly when the network breaks.

**Preconditions**

* Multiplayer lobby or match is active.

### UC-10A – Host disconnects (client-side detection)

**Main Flow**

1. Host closes the game or connection drops.
2. Client stops receiving messages (`StateUpdate` and/or `KeepAlive`).
3. Client detects loss of liveness (timeout via `timeSinceLastHeard()`).
4. Client displays **HOST DISCONNECTED** overlay and disables gameplay.
5. Client can return to Start menu.

**Postconditions**

* Client remains responsive and can recover to menu.

### UC-10B – Client disconnects (host-side detection)

**Main Flow**

1. Client disconnects or closes the game.
2. Host detects disconnect during `NetworkHost::poll()`.
3. Host broadcasts `PlayerLeft(playerId, reason)` to remaining peers (if applicable).
4. Host ends the match and shows an overlay: **Opponent disconnected**.
5. Rematch is disabled for missing players.

**Postconditions**

* Host remains responsive and state remains consistent.
