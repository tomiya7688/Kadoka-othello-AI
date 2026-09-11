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

This is useful for high-volume inference when process-based compatibility is desired without Python overhead.

A reference implementation is provided at:

```text
samples/script_evaluator/native_evaluator.cpp
```

and is built as:

```text
kadoka_native_evaluator_sample
```

### wasm

Reserved in the runtime enum and configuration parser, but execution is not implemented yet.

Using `runtime: "wasm"` currently fails explicitly instead of silently falling back to another runtime.

## Batch protocol

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

## Runtime / Creator boundary

Script evaluation belongs to the AI Runtime because a packaged model may require it to choose a move during a game.

AI Creator may inspect, benchmark and tune a script evaluator, but Runtime must not depend on Creator support code.

## Performance note

`native_process` removes Python interpreter overhead but still has process startup and temporary-file overhead. Native in-process or WASM execution can later provide a lower-latency path while preserving the same logical evaluator interface.
