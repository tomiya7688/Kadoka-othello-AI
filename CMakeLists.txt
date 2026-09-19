cmake_minimum_required(VERSION 3.20)

project(KadokaOthelloAI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(KADOKA_BUILD_AI_CREATOR "Build AI Creator support library and executable" ON)

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
    src/core_state.cpp
    src/game_record.cpp
    src/ai.cpp
    src/model_descriptor.cpp
    src/script_evaluator.cpp
    src/native_in_process_evaluator.cpp
    src/evaluator_ai.cpp
    src/external_ai_session.cpp
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
if(CMAKE_DL_LIBS)
    target_link_libraries(kadoka_othello_runtime PRIVATE ${CMAKE_DL_LIBS})
endif()

add_executable(kadoka_othello_headless
    src/tools/headless_main.cpp
)

target_link_libraries(kadoka_othello_headless
    PRIVATE
        kadoka_othello_runtime
)

add_executable(kadoka_headless_benchmark
    src/tools/headless_benchmark.cpp
)

target_link_libraries(kadoka_headless_benchmark
    PRIVATE
        kadoka_othello_runtime
)

# League orchestration is intentionally outside the runtime library. It uses the
# same packaged AI/headless path without becoming a dependency of game binaries.
add_library(kadoka_othello_league_support
    src/league.cpp
)

target_link_libraries(kadoka_othello_league_support
    PUBLIC
        kadoka_othello_runtime
)

target_compile_features(kadoka_othello_league_support PUBLIC cxx_std_17)

add_executable(kadoka_ai_league
    src/tools/ai_league_main.cpp
)

target_link_libraries(kadoka_ai_league
    PRIVATE
        kadoka_othello_league_support
)

# AI Creator is optional by design. Runtime/headless must remain buildable when
# Creator support is disabled so the development path cannot become a runtime dependency.
if(KADOKA_BUILD_AI_CREATOR)
    add_library(kadoka_othello_creator_support
        src/ai_creator.cpp
        src/package_import.cpp
        src/model_data.cpp
        src/conversion.cpp
        src/dataset_pool.cpp
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

    add_executable(kadoka_othello_ai_creator
        src/tools/ai_creator_main.cpp
    )

    target_link_libraries(kadoka_othello_ai_creator
        PRIVATE
            kadoka_othello_creator_support
    )

    add_executable(kadoka_dataset_tool
        src/tools/dataset_tool_main.cpp
    )

    target_link_libraries(kadoka_dataset_tool
        PRIVATE
            kadoka_othello_creator_support
    )
endif()

add_executable(kadoka_script_evaluator_probe
    src/tools/script_evaluator_probe.cpp
)

target_link_libraries(kadoka_script_evaluator_probe
    PRIVATE
        kadoka_othello_runtime
)

add_executable(kadoka_script_evaluator_benchmark
    src/tools/script_evaluator_benchmark.cpp
)

target_link_libraries(kadoka_script_evaluator_benchmark
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

# Generate runnable evaluator configs in the build tree so no platform-specific
# executable/DLL/SO path needs to be committed to the source tree. Include the
# build configuration in the output path for Visual Studio and other multi-config generators.
file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>/python_evaluator.json"
    CONTENT "{\n  \"format\": \"kadoka.script_evaluator.v1\",\n  \"runtime\": \"python_process\",\n  \"script\": \"${CMAKE_CURRENT_SOURCE_DIR}/samples/script_evaluator/evaluator.py\",\n  \"executable\": \"${Python3_EXECUTABLE}\"\n}\n"
)

file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>/native_process_evaluator.json"
    CONTENT "{\n  \"format\": \"kadoka.script_evaluator.v1\",\n  \"runtime\": \"native_process\",\n  \"program\": \"$<TARGET_FILE:kadoka_native_evaluator_sample>\"\n}\n"
)

file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>/in_process_evaluator.json"
    CONTENT "{\n  \"format\": \"kadoka.script_evaluator.v1\",\n  \"runtime\": \"native_in_process\",\n  \"library\": \"$<TARGET_FILE:kadoka_in_process_evaluator_sample>\",\n  \"symbol\": \"kadoka_script_evaluator_v1\"\n}\n"
)

# A real native package backed by the in-process evaluator. Headless and AI Creator
# load this package through the same package/model path used by distributable AIs.
file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>/evaluator_ai_model.json"
    CONTENT "{\n  \"format\": \"kadoka.model.v1\",\n  \"model_id\": \"kadoka.sample.evaluator_ai.model\",\n  \"model_kind\": \"evaluator_ai_demo\",\n  \"engine\": \"kadoka.evaluator_ai.v1\",\n  \"assets\": [\n    {\n      \"id\": \"evaluator\",\n      \"type\": \"kadoka.script_evaluator.v1\",\n      \"path\": \"in_process_evaluator.json\",\n      \"required\": true\n    }\n  ]\n}\n"
)

file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>/evaluator_ai_manifest.json"
    CONTENT "{\n  \"id\": \"kadoka.sample.evaluator_ai\",\n  \"name\": \"Kadoka Evaluator AI Sample\",\n  \"version\": \"0.1.0\",\n  \"protocol_version\": 1,\n  \"board_sizes\": [6, 8, 10],\n  \"interface\": \"native\",\n  \"entry\": \"builtin:kadoka.evaluator_ai\",\n  \"model\": \"evaluator_ai_model.json\",\n  \"capabilities\": [\"move\", \"inspection\"]\n}\n"
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

    add_executable(kadoka_game_record_tests
        src/tests/game_record_tests.cpp
    )

    target_link_libraries(kadoka_game_record_tests
        PRIVATE
            kadoka_othello_runtime
    )

    add_test(
        NAME kadoka_game_record_tests
        COMMAND kadoka_game_record_tests
    )

    add_test(
        NAME kadoka_game_record_headless_smoke
        COMMAND kadoka_othello_headless
            1
            6
            -
            1818
            -
            -
            "${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>/record_board_state.jsonl"
            "${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>/record_game_aux.jsonl"
    )

    add_executable(kadoka_othello_league_tests
        src/tests/league_tests.cpp
    )

    target_link_libraries(kadoka_othello_league_tests
        PRIVATE
            kadoka_othello_league_support
    )

    add_test(
        NAME kadoka_othello_league_tests
        COMMAND kadoka_othello_league_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/src/packages/random/manifest.json"
    )

    add_test(
        NAME kadoka_ai_league_smoke
        COMMAND kadoka_ai_league
            6
            1
            4242
            "${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>/league_games.jsonl"
            "${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>/league_positions.jsonl"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/packages/random/manifest.json"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/packages/obake_kadoka/manifest.json"
    )

    add_test(
        NAME kadoka_script_evaluator_in_process_smoke
        COMMAND kadoka_script_evaluator_probe
            "${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>/in_process_evaluator.json"
            memory_bonus=2.0
            recall_penalty=0.4
    )

    add_test(
        NAME kadoka_script_evaluator_runtime_benchmark
        COMMAND kadoka_script_evaluator_benchmark
            3
            "${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>/python_evaluator.json"
            "${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>/native_process_evaluator.json"
            "${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>/in_process_evaluator.json"
    )

    add_test(
        NAME kadoka_evaluator_ai_headless_smoke
        COMMAND kadoka_othello_headless
            1
            8
            "${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>/evaluator_ai_headless.jsonl"
            123
            "${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>/evaluator_ai_manifest.json"
            "${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>/evaluator_ai_manifest.json"
    )

    add_test(
        NAME kadoka_obake_6x6_headless_smoke
        COMMAND kadoka_othello_headless
            1
            6
            "${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>/obake_6x6.jsonl"
            6001
            "${CMAKE_CURRENT_SOURCE_DIR}/src/packages/obake_kadoka/manifest.json"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/packages/random/manifest.json"
    )

    add_test(
        NAME kadoka_obake_8x8_headless_smoke
        COMMAND kadoka_othello_headless
            1
            8
            "${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>/obake_8x8.jsonl"
            8001
            "${CMAKE_CURRENT_SOURCE_DIR}/src/packages/obake_kadoka/manifest.json"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/packages/obake_maru/manifest.json"
    )

    add_test(
        NAME kadoka_obake_10x10_headless_smoke
        COMMAND kadoka_othello_headless
            1
            10
            "${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>/obake_10x10.jsonl"
            10001
            "${CMAKE_CURRENT_SOURCE_DIR}/src/packages/random/manifest.json"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/packages/obake_maru/manifest.json"
    )

    if(KADOKA_BUILD_AI_CREATOR)
        add_test(
            NAME kadoka_evaluator_ai_creator_inspect_smoke
            COMMAND kadoka_othello_ai_creator
                analyze
                "${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>/evaluator_ai_manifest.json"
        )

        add_test(
            NAME kadoka_obake_kadoka_creator_inspect_smoke
            COMMAND kadoka_othello_ai_creator
                analyze
                "${CMAKE_CURRENT_SOURCE_DIR}/src/packages/obake_kadoka/manifest.json"
        )

        add_test(
            NAME kadoka_obake_maru_creator_inspect_smoke
            COMMAND kadoka_othello_ai_creator
                analyze
                "${CMAKE_CURRENT_SOURCE_DIR}/src/packages/obake_maru/manifest.json"
        )

        add_executable(kadoka_dataset_pool_tests
            src/tests/dataset_pool_tests.cpp
        )

        target_link_libraries(kadoka_dataset_pool_tests
            PRIVATE
                kadoka_othello_creator_support
        )

        add_test(
            NAME kadoka_dataset_pool_tests
            COMMAND kadoka_dataset_pool_tests
        )

        add_test(
            NAME kadoka_dataset_tool_validate
            COMMAND kadoka_dataset_tool
                validate
                "${CMAKE_CURRENT_SOURCE_DIR}/samples/dataset_pool/registry.jsonl"
        )

        add_test(
            NAME kadoka_dataset_tool_plan
            COMMAND kadoka_dataset_tool
                plan
                "${CMAKE_CURRENT_SOURCE_DIR}/samples/dataset_pool/registry.jsonl"
                "${CMAKE_CURRENT_SOURCE_DIR}/samples/dataset_pool/recipe.json"
        )

        add_test(
            NAME kadoka_dataset_tool_attach_recipe
            COMMAND kadoka_dataset_tool
                attach-recipe
                "${CMAKE_CURRENT_SOURCE_DIR}/src/packages/obake_kadoka/metadata.json"
                "${CMAKE_CURRENT_SOURCE_DIR}/samples/dataset_pool/recipe.json"
                "${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>/dataset_recipe_metadata.json"
        )

        add_test(
            NAME kadoka_dataset_tool_metadata_validation
            COMMAND
                ${Python3_EXECUTABLE}
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/family_metadata/script/validate_metadata.py"
                --game othello
                "${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>/dataset_recipe_metadata.json"
        )

        set_tests_properties(
            kadoka_dataset_tool_metadata_validation
            PROPERTIES
                DEPENDS kadoka_dataset_tool_attach_recipe
        )
    endif()

    add_executable(kadoka_external_ai_session_helper
        src/tests/external_ai_session_helper.cpp
    )

    target_link_libraries(kadoka_external_ai_session_helper
        PRIVATE
            kadoka_othello_runtime
    )

    add_executable(kadoka_external_ai_session_test
        src/tests/external_ai_session_test.cpp
    )

    target_link_libraries(kadoka_external_ai_session_test
        PRIVATE
            kadoka_othello_runtime
    )

    add_test(
        NAME kadoka_external_ai_session_test
        COMMAND kadoka_external_ai_session_test
            "$<TARGET_FILE:kadoka_external_ai_session_helper>"
    )
endif()
