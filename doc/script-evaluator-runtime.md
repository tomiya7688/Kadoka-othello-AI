# Script Evaluator Runtime

`kadoka.script_evaluator.v1` defines the evaluator protocol, not a programming language.

Input is a batch of cases. Each case contains numeric features. Output is a matching batch of results with named numeric values and optional diagnostics.

## Runtime types

### python_process

Implemented.

Example:

```json
{
  "format": "kadoka.script_evaluator.v1",
  "runtime": "python_process",
  "executable": "python",
  "script": "evaluator.py"
}
```

The runtime executes the configured Python interpreter and passes:

```text
--kadoka-eval-input <path>
--kadoka-eval-output <path>
```

### native_process

Implemented.

Example:

```json
{
  "format": "kadoka.script_evaluator.v1",
  "runtime": "native_process",
  "program": "evaluator.exe"
}
```

The native executable receives the same input/output arguments and must implement the same batch protocol as the Python evaluator.

This removes Python interpreter overhead, but process startup and temporary-file I/O remain.

A reference implementation is provided at:

```text
samples/script_evaluator/native_evaluator.cpp
```

and is built as:

```text
kadoka_native_evaluator_sample
```

### native_in_process

Implemented.

Example:

```json
{
  "format": "kadoka.script_evaluator.v1",
  "runtime": "native_in_process",
  "library": "my_evaluator.dll",
  "symbol": "kadoka_script_evaluator_v1"
}
```

On Linux/macOS-style systems the `library` may be a `.so`/compatible dynamic module.
The path is resolved relative to the evaluator config when it is not absolute.

`native_in_process` loads the module once when `ScriptEvaluator` is constructed and calls the exported function directly for each batch. It does not create a child process or temporary input/output files.

The stable C ABI is defined in:

```text
src/kadoka_othello/script_evaluator_abi.h
```

The exported function uses C-compatible input structures and a host-owned callback sink:

```text
cases[]
  -> native module
  -> begin_result(id)
  -> value(key, number)
  -> diagnostic(key, text)
  -> end_result()
```

C++ STL containers are intentionally not passed across the dynamic-module boundary.
The host copies emitted strings during the callback, so module-owned output strings only need to remain valid for the duration of the callback.

A reference implementation is provided at:

```text
samples/script_evaluator/in_process_evaluator.cpp
```

and is built as:

```text
kadoka_in_process_evaluator_sample
```

CMake also generates a platform-correct evaluator config under the build tree:

```text
build/generated/<configuration>/in_process_evaluator.json
```

For Visual Studio Release builds this is normally:

```text
build/generated/Release/in_process_evaluator.json
```

Single-config generators may use an empty or build-type-specific configuration directory.

### wasm

Reserved in the runtime enum and configuration parser, but execution is not implemented yet.

Using `runtime: "wasm"` currently fails explicitly instead of silently falling back to another runtime.

## Evaluator-backed AI engine

`kadoka.evaluator_ai.v1` connects a `kadoka.script_evaluator.v1` asset to the normal AI package path.
It is a Runtime engine, not a Creator-only helper.

A model selects it with:

```json
{
  "format": "kadoka.model.v1",
  "model_id": "my.evaluator.model",
  "model_kind": "evaluator_ai",
  "engine": "kadoka.evaluator_ai.v1",
  "assets": [
    {
      "id": "evaluator",
      "type": "kadoka.script_evaluator.v1",
      "path": "evaluator.json",
      "required": true
    }
  ]
}
```

For each legal move, Runtime creates one evaluator case. The current stable feature set is:

```text
row
col
row_normalized
col_normalized
is_corner
is_edge
center_distance
legal_move_count
```

All legal candidates are sent in one batch. The evaluator should return `score`; optional `policy` or `confidence` is exposed through `AIInspection`.
The highest `score` becomes the move proposal. Runtime still applies the normal game legality check before canonical state changes.

`inspect()` exposes every evaluated candidate plus diagnostics, so AI Creator and Headless use the exact same evaluator-backed engine:

```text
manifest.json
  -> model.json
  -> evaluator.json
  -> EvaluatorAI
       -> ScriptEvaluator batch
       -> AIOutput / AIInspection
```

CMake generates a complete native-in-process sample package in the build tree:

```text
build/generated/<configuration>/evaluator_ai_manifest.json
build/generated/<configuration>/evaluator_ai_model.json
build/generated/<configuration>/in_process_evaluator.json
```

CTest loads that package through both:

- `kadoka_othello_headless`
- `kadoka_othello_ai_creator analyze`

This verifies that the low-latency evaluator path is part of the normal Runtime package flow instead of only being reachable through the standalone probe.

## Process batch protocol

`python_process` and `native_process` use the following file protocol.

Input:

```text
KADOKA_SCRIPT_EVALUATOR 1
case 0
feature memory_bonus 2.0
feature recall_delay 0.4
end
case 1
feature memory_bonus 0.8
feature recall_delay 0.1
end
```

Output:

```text
result 0
value score 1.6
value confidence 1.0
diag runtime=native_process
end
result 1
value score 0.7
value confidence 1.0
end
```

The evaluator is called once per batch rather than once per candidate.

`native_in_process` preserves the same logical `ScriptEvaluatorCase` / `ScriptEvaluatorResult` contract, but does not serialize it through files.

## Benchmark

The single-runtime probe can execute an evaluator repeatedly:

```text
kadoka_script_evaluator_probe <evaluator.json> --repeat 1000 memory_bonus=2 recall_penalty=0.4
```

For side-by-side comparison, the build also provides:

```text
kadoka_script_evaluator_benchmark \
  <repeat> \
  <python-config> \
  <native-process-config> \
  <native-in-process-config>
```

It prints one line per runtime:

```text
runtime=python_process repeat=... total_us=... average_us=...
runtime=native_process repeat=... total_us=... average_us=...
runtime=native_in_process repeat=... total_us=... average_us=...
```

Linux CI runs this benchmark with identical input and emits the values into the Actions log. The benchmark is evidence, not a timing threshold: hosted-runner noise must not turn performance measurements into flaky correctness failures.

The first run may include process/module initialization effects, so record the comparison conditions when performance is used as evidence.

## Runtime / Creator boundary

Script evaluation belongs to the AI Runtime because a packaged model may require it to choose a move during a game.

AI Creator may inspect, benchmark and tune a script evaluator, but Runtime must not depend on Creator support code.

## Trust boundary

`native_in_process` loads executable native code into the game process. Treat such model assets with the same trust level as native plugins or other executable code.

Do not silently load an in-process library from an untrusted package. Sandboxed/untrusted distribution is a separate concern from the low-latency native path.

## Performance rule

Prefer the least expensive runtime that satisfies the model's portability and trust requirements:

```text
native_in_process
  -> no process startup / no temp-file I/O

native_process
  -> native executable, process isolation remains

python_process
  -> easiest to author/tune, highest process/interpreter overhead

wasm
  -> reserved for portable/sandbox-oriented execution
```
