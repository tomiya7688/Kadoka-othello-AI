# Script Evaluator

`kadoka.script_evaluator.v1` is an executable model asset for AI models that carry their own evaluation script.

Current runtime:

- `python_process`

A model root can reference it like this:

```json
{
  "id": "evaluator",
  "type": "kadoka.script_evaluator.v1",
  "path": "evaluator.json",
  "required": true
}
```

The evaluator config points to the script:

```json
{
  "format": "kadoka.script_evaluator.v1",
  "runtime": "python_process",
  "executable": "python",
  "script": "evaluator.py"
}
```

## Batch protocol

The C++ runtime sends every candidate for one evaluation step in one process call.

Input:

```text
KADOKA_SCRIPT_EVALUATOR 1
case 0
feature memory_bonus 2.0
feature recall_delay 0.4
end
case 1
feature memory_bonus 1.0
feature recall_delay 0.1
end
```

Output:

```text
result 0
value score 1.6
value confidence 0.8
diag source=script
end
result 1
value score 0.9
value confidence 0.6
end
```

Output names are arbitrary numeric channels. `score` is a convention, not the only permitted value.

This allows a model such as Sage Merry Slime to keep memory/path handling in its native engine while its package-owned evaluator script receives extracted features and returns score, confidence, forgetting modifiers, or other numeric outputs.

## Runtime API

C++ provides:

- `ScriptEvaluator`
- `ScriptEvaluatorCase`
- `ScriptEvaluatorResult`
- `load_script_evaluator_config()`
- `load_script_evaluator_asset()`

`load_script_evaluator_asset()` resolves a `kadoka.script_evaluator.v1` asset directly from `ModelRootDescriptor`.

A runnable reference exists under `samples/script_evaluator/`.

The probe executable can be used without an AI engine:

```bat
build\Release\kadoka_script_evaluator_probe.exe ^
  samples\script_evaluator\evaluator.json ^
  memory_bonus=2.0 ^
  recall_delay=0.4
```

The current implementation intentionally batches candidate evaluation to avoid launching Python once per move candidate.
