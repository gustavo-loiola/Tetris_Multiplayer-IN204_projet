set(TEST_SOURCES
    test_gameplay_core.cpp
    test_multiplayer_modes.cpp
    test_network.cpp
    test_scoring_and_lines.cpp
    test_state_update_mapper.cpp
)

add_executable(tetris_tests
    ${TEST_SOURCES}
)

target_link_libraries(tetris_tests
    PRIVATE 
    tetris_core
    tetris_net
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
