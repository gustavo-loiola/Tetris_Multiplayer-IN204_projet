cmake_minimum_required(VERSION 3.16)
project(MultiplayerTetris LANGUAGES CXX)
include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

add_executable(tetris_console
    src/main_console.cpp
)

target_link_libraries(tetris_console
    PRIVATE tetris_core
)

target_include_directories(tetris_console
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)

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
