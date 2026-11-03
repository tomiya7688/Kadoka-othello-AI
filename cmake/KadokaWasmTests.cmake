# These tools are test dependencies, never dependencies of Runtime distribution.
set(KADOKA_WASI_SDK_ROOT
    ""
    CACHE PATH "WASI SDK installation used to build test modules")
if(NOT KADOKA_WASI_SDK_ROOT)
    message(
        FATAL_ERROR
            "WASM tests require -DKADOKA_WASI_SDK_ROOT=<WASI SDK directory>")
endif()
find_program(
    KADOKA_WASI_CXX
    NAMES clang++
    HINTS "${KADOKA_WASI_SDK_ROOT}/bin"
    NO_DEFAULT_PATH REQUIRED)
find_program(KADOKA_WASMTIME_EXECUTABLE NAMES wasmtime REQUIRED)

# A space in the module path exercises host command quoting on both platforms.
set(_kadoka_wasm_dir
    "${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>/wasm samples")
string(REPLACE "\"" "\\\"" _kadoka_cache_dir
               "${CMAKE_CURRENT_BINARY_DIR}/wasmtime-cache")
file(
    GENERATE
    OUTPUT "${_kadoka_wasm_dir}/wasmtime-cache.toml"
    CONTENT "[cache]\ndirectory = \"${_kadoka_cache_dir}\"\n")
set(_kadoka_wasm_outputs)
foreach(_kadoka_fixture IN ITEMS wasm_evaluator wasm_evaluator_failure)
    if(_kadoka_fixture STREQUAL "wasm_evaluator")
        set(_kadoka_source
            "${CMAKE_CURRENT_SOURCE_DIR}/samples/script_evaluator/wasm_evaluator.cpp"
        )
    else()
        set(_kadoka_source
            "${CMAKE_CURRENT_SOURCE_DIR}/src/tests/wasm_evaluator_failure.cpp")
    endif()
    set(_kadoka_output "${_kadoka_wasm_dir}/${_kadoka_fixture}.wasm")
    add_custom_command(
        OUTPUT "${_kadoka_output}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_kadoka_wasm_dir}"
        COMMAND "${KADOKA_WASI_CXX}" --target=wasm32-wasip1 -std=c++17 -O2
                -fno-exceptions "${_kadoka_source}" -o "${_kadoka_output}"
        DEPENDS "${_kadoka_source}"
        COMMENT "Building WASI module ${_kadoka_fixture}"
        VERBATIM)
    list(APPEND _kadoka_wasm_outputs "${_kadoka_output}")
    file(
        GENERATE
        OUTPUT "${_kadoka_wasm_dir}/${_kadoka_fixture}.json"
        CONTENT
            "{\n  \"format\": \"kadoka.script_evaluator.v1\",\n  \"runtime\": \"wasm\",\n  \"module\": \"${_kadoka_fixture}.wasm\",\n  \"executable\": \"${KADOKA_WASMTIME_EXECUTABLE}\",\n  \"cache_config\": \"wasmtime-cache.toml\"\n}\n"
    )
endforeach()
add_custom_target(kadoka_wasm_evaluator_samples ALL
                  DEPENDS ${_kadoka_wasm_outputs})

file(
    GENERATE
    OUTPUT "${_kadoka_wasm_dir}/model.json"
    CONTENT
        [=[
{
  "format": "kadoka.model.v1",
  "model_id": "kadoka.sample.wasm.model",
  "model_kind": "evaluator_ai_demo",
  "engine": "kadoka.evaluator_ai.v1",
  "assets": [
    {"id": "evaluator", "type": "kadoka.script_evaluator.v1", "path": "wasm_evaluator.json", "required": true}
  ]
}
]=])
file(
    GENERATE
    OUTPUT "${_kadoka_wasm_dir}/manifest.json"
    CONTENT
        [=[
{
  "id": "kadoka.sample.wasm",
  "name": "Kadoka WASM Evaluator Sample",
  "version": "0.1.0",
  "protocol_version": 1,
  "board_sizes": [6, 8, 10],
  "interface": "native",
  "entry": "builtin:kadoka.evaluator_ai",
  "model": "model.json",
  "capabilities": ["move", "inspection"]
}
]=])

add_executable(kadoka_wasm_evaluator_tests src/tests/wasm_evaluator_tests.cpp)
target_link_libraries(kadoka_wasm_evaluator_tests
                      PRIVATE kadoka_othello_runtime)
if(MSVC)
    target_compile_options(kadoka_wasm_evaluator_tests PRIVATE /EHsc)
endif()
add_dependencies(kadoka_wasm_evaluator_tests kadoka_wasm_evaluator_samples
                 kadoka_native_evaluator_sample)

add_test(
    NAME kadoka_wasm_evaluator_contract
    COMMAND
        kadoka_wasm_evaluator_tests "${_kadoka_wasm_dir}/wasm_evaluator.json"
        "${_kadoka_wasm_dir}/wasm_evaluator_failure.json"
        "${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>/python_evaluator.json"
        "${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>/native_process_evaluator.json"
)
set_tests_properties(kadoka_wasm_evaluator_contract PROPERTIES TIMEOUT 60)
add_test(
    NAME kadoka_wasm_headless_smoke
    COMMAND
        kadoka_othello_headless 1 6 "" 12345
        "${_kadoka_wasm_dir}/manifest.json" "${_kadoka_wasm_dir}/manifest.json")
set_tests_properties(kadoka_wasm_headless_smoke PROPERTIES TIMEOUT 120)
if(KADOKA_BUILD_AI_CREATOR)
    add_test(NAME kadoka_wasm_creator_inspect_smoke
             COMMAND kadoka_othello_ai_creator analyze
                     "${_kadoka_wasm_dir}/manifest.json")
    set_tests_properties(kadoka_wasm_creator_inspect_smoke PROPERTIES TIMEOUT
                                                                      60)
endif()
