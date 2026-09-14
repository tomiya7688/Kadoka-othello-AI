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

## Benchmark probe

The probe can execute the same evaluator repeatedly:

```text
kadoka_script_evaluator_probe <evaluator.json> --repeat 1000 memory_bonus=2 recall_penalty=0.4
```

It prints:

```text
benchmark.repeat=1000
benchmark.total_us=...
benchmark.average_us=...
```

For runtime comparisons, use identical feature input and repeat count. The first run may include process/module initialization effects, so record the comparison conditions when performance is used as evidence.

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
