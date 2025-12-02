cmake_minimum_required(VERSION 3.16)
project(MultiplayerTetris LANGUAGES CXX)
include(FetchContent)

# --- vcpkg integration (optional, but handy for cross-machine builds) ---
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    # Use vcpkg toolchain if the user has VCPKG_ROOT set
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# =========================
# Core library
# =========================
add_library(tetris_core
    src/core/Board.cpp
    src/core/Tetromino.cpp
    src/core/TetrominoFactory.cpp
    src/core/GameState.cpp
    src/core/ScoreManager.cpp
    src/core/LevelManager.cpp
    src/controller/GameController.cpp
    src/core/TimeAttackRules.cpp
    src/core/SharedTurnRules.cpp
)

target_include_directories(tetris_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# =========================
# Network library (messages + serialization)
# =========================
add_library(tetris_net
    src/network/Serialization.cpp
    src/network/NetworkClient.cpp
    src/network/NetworkHost.cpp
    src/network/HostGameSession.cpp
    src/network/StateUpdateMapper.cpp
    src/network/HostLoop.cpp
    src/network/TcpSession.cpp    
    src/network/TcpServer.cpp
)

target_include_directories(tetris_net
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        tetris_core
)

if (WIN32)
    target_link_libraries(tetris_net PRIVATE ws2_32)
endif()

# If later the network layer needs symbols from tetris_core
# (e.g. converting GameState <-> DTOs), we can add:
# target_link_libraries(tetris_net PUBLIC tetris_core)

# =========================
# Console executable
# =========================
add_executable(tetris_console
    src/main_console.cpp
)

target_link_libraries(tetris_console
    PRIVATE tetris_core
)

target_include_directories(tetris_console
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# =========================
# GUI executable (wxWidgets)
# =========================
option(BUILD_GUI "Build GUI executable" ON)

if(BUILD_GUI)
    # Use config-mode find_package provided by vcpkg
    find_package(wxWidgets CONFIG REQUIRED)

    add_executable(tetris_gui WIN32
        src/gui/TetrisApp.cpp
        src/gui/TetrisFrame.cpp
        src/gui/BoardPanel.cpp
        src/gui/NextPiecePanel.cpp
        src/gui/StartFrame.cpp
        src/gui/MultiplayerConfigDialog.cpp
        src/gui/RemoteBoardPanel.cpp          
        src/gui/MultiplayerFrame.cpp
    )

    target_include_directories(tetris_gui
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(tetris_gui
        PRIVATE
            tetris_core
            tetris_net
            wx::core
            wx::base
    )
endif()

# =========================
# Tests (Catch2)
# =========================
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.11.0  # or any recent stable v3 tag
)

option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
    enable_testing()
    FetchContent_MakeAvailable(Catch2)
    add_subdirectory(tests)
endif()