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
)

target_include_directories(tetris_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

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
    )

    target_include_directories(tetris_gui
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(tetris_gui
        PRIVATE
            tetris_core
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