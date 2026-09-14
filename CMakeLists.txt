cmake_minimum_required(VERSION 3.20)

project(KadokaOthelloAI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_custom_target(kadoka_rule_check
    COMMAND
        ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/kadoka_rule_checker/script/kadoka_rule_checker.py
        ${CMAKE_CURRENT_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Checking Kadoka coding rules"
    VERBATIM
)

# Runtime-only library used by games, headless execution and packaged AIs.
# Keep Creator analysis/conversion/import code out of this target so hot-path
# binaries do not carry development-tool dependencies.
add_library(kadoka_othello_runtime
    src/board.cpp
    src/rules.cpp
    src/game.cpp
    src/state.cpp
    src/ai.cpp
    src/model_descriptor.cpp
    src/script_evaluator.cpp
    src/native_in_process_evaluator.cpp
    src/obake_kadoka_evaluator.cpp
    src/obake_kadoka_model.cpp
    src/obake_kadoka.cpp
    src/obake_maru_model.cpp
    src/obake_maru.cpp
    src/package.cpp
    src/package_loader.cpp
    src/headless.cpp
)

add_dependencies(kadoka_othello_runtime kadoka_rule_check)

target_include_directories(kadoka_othello_runtime
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(kadoka_othello_runtime PUBLIC cxx_std_17)
target_link_libraries(kadoka_othello_runtime PRIVATE ${CMAKE_DL_LIBS})

# AI Creator support library. This may depend on the runtime, but the runtime
# must never depend on this target.
add_library(kadoka_othello_creator_support
    src/ai_creator.cpp
    src/package_import.cpp
    src/model_data.cpp
    src/conversion.cpp
)

target_include_directories(kadoka_othello_creator_support
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(kadoka_othello_creator_support PUBLIC cxx_std_17)

target_link_libraries(kadoka_othello_creator_support
    PUBLIC
        kadoka_othello_runtime
)

add_executable(kadoka_othello_headless
    src/tools/headless_main.cpp
)

target_link_libraries(kadoka_othello_headless
    PRIVATE
        kadoka_othello_runtime
)

add_executable(kadoka_othello_ai_creator
    src/tools/ai_creator_main.cpp
)

target_link_libraries(kadoka_othello_ai_creator
    PRIVATE
        kadoka_othello_creator_support
)

add_executable(kadoka_script_evaluator_probe
    src/tools/script_evaluator_probe.cpp
)

target_link_libraries(kadoka_script_evaluator_probe
    PRIVATE
        kadoka_othello_runtime
)

# Reference implementation for runtime=native_process.
add_executable(kadoka_native_evaluator_sample
    samples/script_evaluator/native_evaluator.cpp
)

# Reference implementation for runtime=native_in_process.
add_library(kadoka_in_process_evaluator_sample SHARED
    samples/script_evaluator/in_process_evaluator.cpp
)

target_include_directories(kadoka_in_process_evaluator_sample
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(kadoka_in_process_evaluator_sample PUBLIC cxx_std_17)

# Generate a runnable evaluator config in the build tree so no platform-specific
# DLL/SO path needs to be committed to the source tree.
file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/in_process_evaluator.json"
    CONTENT "{\n  \"format\": \"kadoka.script_evaluator.v1\",\n  \"runtime\": \"native_in_process\",\n  \"library\": \"$<TARGET_FILE:kadoka_in_process_evaluator_sample>\",\n  \"symbol\": \"kadoka_script_evaluator_v1\"\n}\n"
)

include(CTest)

if(BUILD_TESTING)
    add_executable(kadoka_othello_core_tests
        src/tests/core_tests.cpp
    )

    target_link_libraries(kadoka_othello_core_tests
        PRIVATE
            kadoka_othello_runtime
    )

    add_test(
        NAME kadoka_othello_core_tests
        COMMAND kadoka_othello_core_tests
    )

    add_test(
        NAME kadoka_script_evaluator_in_process_smoke
        COMMAND kadoka_script_evaluator_probe
            "${CMAKE_CURRENT_BINARY_DIR}/in_process_evaluator.json"
            memory_bonus=2.0
            recall_penalty=0.4
    )
endif()
