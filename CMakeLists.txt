cmake_minimum_required(VERSION 3.20)

project(KadokaOthelloAI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(kadoka_othello_core
    src/board.cpp
    src/rules.cpp
    src/game.cpp
)

target_include_directories(kadoka_othello_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(kadoka_othello_core PUBLIC cxx_std_17)

include(CTest)

if(BUILD_TESTING)
    add_executable(kadoka_othello_core_tests
        tests/core_tests.cpp
    )

    target_link_libraries(kadoka_othello_core_tests
        PRIVATE
            kadoka_othello_core
    )

    add_test(
        NAME kadoka_othello_core_tests
        COMMAND kadoka_othello_core_tests
    )
endif()
