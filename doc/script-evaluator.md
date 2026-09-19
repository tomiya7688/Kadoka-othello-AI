# Script Evaluator

`kadoka.script_evaluator.v1` はmodelが自身のevaluation logicをassetとして持つためのcontract。

実装済みruntime:

- `python_process`
- `native_process`
- `native_in_process`

予約:

- `wasm`

詳細は `doc/script-evaluator-runtime.md`。

## Asset

```json
{
  "id": "evaluator",
  "type": "kadoka.script_evaluator.v1",
  "path": "evaluator.json",
  "required": true
}
```

例:

```json
{
  "format": "kadoka.script_evaluator.v1",
  "runtime": "python_process",
  "executable": "python",
  "script": "evaluator.py"
}
```

## Batch protocol

process runtimeでは1 evaluation stepのcandidateを1 batchで送る。

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

numeric output channel名はmodel側で追加できる。`score` はconvention。

Sage Merry Slimeのようなmodelでは、memory/path handlingをnative engineに残し、package-owned evaluatorへfeatureを渡してscore/confidence/forget modifier等を返す構成が可能。

## Runtime API

- `ScriptEvaluator`
- `ScriptEvaluatorCase`
- `ScriptEvaluatorResult`
- `load_script_evaluator_config()`
- `load_script_evaluator_asset()`

`load_script_evaluator_asset()` は `ModelRootDescriptor` からassetを解決する。

referenceは `samples/script_evaluator/`。

probe:

```bat
build\Release\kadoka_script_evaluator_probe.exe ^
  samples\script_evaluator\evaluator.json ^
  memory_bonus=2.0 ^
  recall_delay=0.4
```

candidateごとのprocess launchを避け、batch evaluationする。
