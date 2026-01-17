cmake_minimum_required(VERSION 3.16)

# --- Always use Vcpkg toolchain ---
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
endif()

project(MultiplayerTetris LANGUAGES CXX)
include(FetchContent)

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
# GUI executable (wxWidgets + NanoSVG)
# =========================
option(BUILD_GUI "Build GUI executable" ON)

if(BUILD_GUI)
    # wxWidgets via vcpkg
    find_package(wxWidgets CONFIG REQUIRED)
    
    # NanoSVG via vcpkg
    find_package(NanoSVG CONFIG REQUIRED)

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
            NanoSVG::nanosvg
            NanoSVG::nanosvgrast
    )
endif()


# =========================
# SDL2 + ImGui GUI executable
# =========================
option(BUILD_SDL_GUI "Build SDL2 + ImGui GUI executable" ON)
if(BUILD_SDL_GUI)

    if(NOT DEFINED VCPKG_TARGET_TRIPLET)
        set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "Vcpkg target triplet")
    endif()

    find_package(SDL2 CONFIG REQUIRED)
    find_package(imgui CONFIG REQUIRED)
    
    add_executable(tetris_gui_sdl
        src/gui_sdl/main_sdl.cpp
        src/gui_sdl/Application.cpp
        src/gui_sdl/StartScreen.cpp
        src/gui_sdl/SinglePlayerScreen.cpp
        src/gui_sdl/MultiplayerConfigScreen.cpp
        src/gui_sdl/LobbyScreen.cpp
        src/gui_sdl/MultiplayerGameScreen.cpp
    )
    
    target_include_directories(tetris_gui_sdl
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    
    target_link_libraries(tetris_gui_sdl
        PRIVATE
            tetris_core
            tetris_net
            SDL2::SDL2
            imgui::imgui
    )

    if(TARGET SDL2::SDL2main)
        target_link_libraries(tetris_gui_sdl PRIVATE SDL2::SDL2main)
    endif()

    if (WIN32)
        target_link_libraries(tetris_gui_sdl PRIVATE ws2_32)
    endif()
endif()

# =========================
# Tests (Catch2)
# =========================
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.11.0
)

option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
    enable_testing()
    FetchContent_MakeAvailable(Catch2)
    add_subdirectory(tests)
endif()
