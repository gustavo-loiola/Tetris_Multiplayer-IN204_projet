#pragma once

namespace tetris::controller {

// Discrete player input actions.
// These are UI- and platform-agnostic: keyboard, gamepad, network, etc.
enum class InputAction {
    MoveLeft,
    MoveRight,
    SoftDrop,
    HardDrop,
    RotateCW,
    RotateCCW,
    PauseResume
};

} // namespace tetris::controller
