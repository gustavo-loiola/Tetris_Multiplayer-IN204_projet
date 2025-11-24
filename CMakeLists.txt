cmake_minimum_required(VERSION 3.16)
project(MultiplayerTetris LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTS "Build unit tests" ON)

add_library(tetris_core
    src/core/Board.cpp
    src/core/Tetromino.cpp
    src/core/TetrominoFactory.cpp
    src/core/GameState.cpp
    src/core/ScoreManager.cpp
    src/core/LevelManager.cpp
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

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
