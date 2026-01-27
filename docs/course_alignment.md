# Course Alignment – IN204 Object-Oriented Programming

**Multiplayer Tetris Project**

This document explains how the **Multiplayer Tetris** project aligns with the concepts, techniques, and skills taught in the **IN204 – Object-Oriented Programming** course.
The alignment is **intentional and structural**: course concepts are applied naturally as part of the system design, not artificially added.

---

## 1. Overall Alignment Summary

The project is a **complete object-oriented system** developed in modern C++ that demonstrates:

* modularization through classes,
* clear object lifetime management (stack vs heap),
* clean architectural separation (core / controller / network / UI),
* inheritance, extension, and polymorphism through interfaces,
* extensibility via design patterns (Strategy, Factory),
* use of templates through the C++ standard library,
* controlled use of operator overloading and streams,
* explicit behavioral contracts and invariants,
* exception-aware and robust design,
* parallelism and asynchronous event-driven networking,
* safe memory management with RAII and smart pointers.

As such, the project fully satisfies the objectives of the IN204 course.

---

## 2. Module-by-Module Alignment

---

## MODULE 1 — Foundations of C++ Object-Oriented Programming

### 1.1 Modularization Through Classes

The project is decomposed into coherent modules implemented as classes:

* **Core domain**:
  `Board`, `Tetromino`, `GameState`, `ScoreManager`, `LevelManager`
* **Control layer**:
  `GameController`
* **Networking**:
  `NetworkHost`, `NetworkClient`, `TcpSession`, `HostGameSession`
* **User interface**:
  `Application`, `Screen`, `StartScreen`, `MultiplayerGameScreen`, etc.

Each class has a **single, well-defined responsibility**, making the system easier to understand, test, and evolve.

---

### 1.2 Classes and Objects

The project models the Tetris domain using objects that encapsulate both **state** and **behavior**:

* `Board` owns the grid and enforces collision rules.
* `Tetromino` encapsulates geometry and rotation logic.
* `GameState` owns the complete gameplay state and lifecycle.
* `GameController` orchestrates time and gravity independently of rules.

This reflects a faithful application of object-oriented modeling.

---

### 1.3 Object Creation: Stack vs Heap

Objects are created on the **stack** or **heap** depending on ownership and lifetime:

**Stack allocation (automatic lifetime):**

```cpp
tetris::core::GameState localGame_;
tetris::controller::GameController localCtrl_;
```

(from `MultiplayerGameScreen`)

**Heap allocation (dynamic lifetime, polymorphism):**

```cpp
std::unique_ptr<Screen> m_screen;
std::shared_ptr<tetris::net::NetworkClient> client_;
```

This demonstrates conscious design choices regarding object lifetime and ownership.

---

### 1.4 Constructors (Implicit and Explicit) and Destructors

The project uses:

* **explicit constructors** to enforce invariants:

```cpp
GameState(int rows = 20, int cols = 10, int startingLevel = 0);
NetworkClient(INetworkSessionPtr session, std::string playerName);
```

* **implicit/default constructors** for simple value types:

```cpp
struct Position {
    int row{};
    int col{};
};
```

Destruction is handled deterministically via RAII, ensuring clean resource release.

---

### 1.5 Visibility and Encapsulation

Visibility rules are applied consistently:

```cpp
class GameState {
public:
    void moveLeft();
    void rotateClockwise();

private:
    Board board_;
    std::optional<Tetromino> activeTetromino_;
};
```

Internal state is protected, and external code can only interact through controlled public APIs.
This enforces invariants and prevents invalid state manipulation.

---

## MODULE 2 — Inheritance, Extension, and Derivation

### 2.1 Class Extension and Derivation

The project uses inheritance to **extend behavior**, not to share implementation blindly:

* UI:

```cpp
class Screen { ... };
class MultiplayerGameScreen : public Screen { ... };
```

* Match rules:

```cpp
class TimeAttackRules : public IMatchRules { ... };
class SharedTurnRules : public IMatchRules { ... };
```

Derived classes extend base behavior while respecting the base contract.

---

### 2.2 Method Overriding (Surcharge)


Derived classes override virtual methods to specialize behavior:

```cpp
void render(Application& app) override;
std::vector<MatchResult> update(...) override;
```

This corresponds to method overriding (often referred to as *surcharge* in course material).

---

## MODULE 3 — Inheritance and Polymorphism

### 3.1 Polymorphism

Polymorphism is central to the architecture:

* `Screen*` → concrete screens
* `IMatchRules*` → match rule strategies
* `INetworkSession*` → TCP implementation

Example:

```cpp
std::unique_ptr<Screen> m_screen;
```

The concrete behavior is resolved at runtime via virtual dispatch.

---

### 3.2 Virtual Destructors

All interface base classes declare virtual destructors, ensuring correct destruction when accessed polymorphically.

---

## MODULE 4 — Templates and Generic Programming

### 4.1 Function and Class Templates (via STL)

**Aligned through standard library usage**

The project makes extensive use of templates provided by the C++ standard library:

* `std::vector<T>`
* `std::array<T, N>`
* `std::optional<T>`
* `std::variant<Ts...>`
* `std::function<>`

Example:

```cpp
using MessagePayload = std::variant<
    JoinRequest,
    StartGame,
    StateUpdate,
    MatchResult,
    ErrorMessage
>;
```

This demonstrates practical and type-safe generic programming.

---

## MODULE 5 — Containers and Iterators

STL containers replace manual memory management and raw arrays:

* `std::vector` for grids and snapshots
* `std::unordered_map` for player/session lookup
* `std::optional` for nullable state

Iteration is abstracted away from representation, improving maintainability.

---

## MODULE 6 — Operator Overloading, Streams, and I/O

### 6.1 Operator Overloading

Operator overloading is intentionally limited to avoid unnecessary complexity.
This design choice favors readability and maintainability.

---

### 6.2 Streams and Data Flow

The networking layer demonstrates stream-based I/O concepts:

* text-based message serialization,
* line-based framing over TCP,
* explicit parsing and validation.

Example:

```cpp
std::string serialize(const Message& msg);
std::optional<Message> deserialize(const std::string& line);
```

---

## MODULE 7 — Constraints and Contracts

The project enforces **explicit contracts** through architecture and interfaces:

* `GameController` does not own `GameState`.
* Clients never simulate gameplay.
* Only the host can start a match.
* In Shared Turns, only the current player’s inputs are applied.

These constraints are enforced structurally, not by convention.


---

## MODULE 8 — Memory Management and Smart Pointers

The project avoids raw `new` / `delete`.

Ownership is explicit:

* `std::unique_ptr` for exclusive ownership (screens, rules),
* `std::shared_ptr` for shared lifetime (network sessions),
* RAII ensures deterministic cleanup.

This guarantees memory safety and clarity of ownership.

---

## MODULE 9 — Parallelism and Asynchronism

### 9.1 Parallelism (Threads)

Networking runs on background threads:

* TCP accept loop (`TcpServer`)
* TCP read loop (`TcpSession`)

```cpp
std::thread m_thread;
std::atomic<bool> m_running;
```

---

### 9.2 Asynchronous Design

The system is event-driven:

* network messages trigger callbacks,
* snapshots arrive asynchronously,
* UI consumes the latest authoritative state.

Synchronization is handled using `std::mutex` and scoped locks to avoid data races.