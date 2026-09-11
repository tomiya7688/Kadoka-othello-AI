cmake_minimum_required(VERSION 3.20)

project(KadokaOthelloAI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(kadoka_othello_core
    src/board.cpp
    src/rules.cpp
    src/game.cpp
    src/state.cpp
    src/ai.cpp
    src/model_descriptor.cpp
    src/script_evaluator.cpp
    src/obake_kadoka_evaluator.cpp
    src/obake_kadoka_model.cpp
    src/obake_kadoka.cpp
    src/obake_maru_model.cpp
    src/obake_maru.cpp
    src/ai_creator.cpp
    src/package.cpp
    src/package_loader.cpp
    src/package_import.cpp
    src/model_data.cpp
    src/conversion.cpp
    src/headless.cpp
)

target_include_directories(kadoka_othello_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(kadoka_othello_core PUBLIC cxx_std_17)

add_executable(kadoka_othello_headless
    src/tools/headless_main.cpp
)

target_link_libraries(kadoka_othello_headless
    PRIVATE
        kadoka_othello_core
)

add_executable(kadoka_othello_ai_creator
    src/tools/ai_creator_main.cpp
)

target_link_libraries(kadoka_othello_ai_creator
    PRIVATE
        kadoka_othello_core
)

add_executable(kadoka_script_evaluator_probe
    src/tools/script_evaluator_probe.cpp
)

target_link_libraries(kadoka_script_evaluator_probe
    PRIVATE
        kadoka_othello_core
)

include(CTest)

if(BUILD_TESTING)
    add_executable(kadoka_othello_core_tests
        src/tests/core_tests.cpp
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
