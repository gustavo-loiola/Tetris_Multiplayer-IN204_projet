set(TEST_SOURCES
    test_score.cpp
    test_board.cpp
    test_game_state.cpp
    test_controller.cpp
)

add_executable(tetris_tests
    ${TEST_SOURCES}
)

target_link_libraries(tetris_tests
    PRIVATE 
    tetris_core
    Catch2::Catch2WithMain
)

target_include_directories(tetris_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}     # for catch.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

include(Catch)
catch_discover_tests(tetris_tests)

add_test(NAME tetris_tests COMMAND tetris_tests)
