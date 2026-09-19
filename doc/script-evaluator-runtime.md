# Script Evaluator Runtime

`kadoka.script_evaluator.v1` はprogramming languageではなくevaluator contractを定義する。

inputはnumeric featureを持つcase batch。

outputは同じIDのresult batchで、named numeric value + optional diagnosticsを返す。

## Runtime type

### python_process

実装済み。

```json
{
  "format": "kadoka.script_evaluator.v1",
  "runtime": "python_process",
  "executable": "python",
  "script": "evaluator.py"
}
```

```text
--kadoka-eval-input <path>
--kadoka-eval-output <path>
```

### native_process

実装済み。

```json
{
  "format": "kadoka.script_evaluator.v1",
  "runtime": "native_process",
  "program": "evaluator.exe"
}
```

Pythonと同じbatch file protocol。

interpreter overheadは減るがprocess startup/temp-file I/Oは残る。

reference: `samples/script_evaluator/native_evaluator.cpp`

### native_in_process

実装済み。

```json
{
  "format": "kadoka.script_evaluator.v1",
  "runtime": "native_in_process",
  "library": "my_evaluator.dll",
  "symbol": "kadoka_script_evaluator_v1"
}
```

moduleを `ScriptEvaluator` construction時に1回loadし、batchごとにexport functionを直接callする。

child process/temp fileなし。

stable C ABI:

```text
src/kadoka_othello/script_evaluator_abi.h
```

```text
cases[]
  -> native module
  -> begin_result(id)
  -> value(key, number)
  -> diagnostic(key, text)
  -> end_result()
```

C++ STL containerをmodule ABI越しに渡さない。

reference: `samples/script_evaluator/in_process_evaluator.cpp`

CMake generated config:

```text
build/generated/<configuration>/in_process_evaluator.json
```

### wasm

enum/config parserでは予約済み。実行は未実装。

`runtime: "wasm"` は他runtimeへsilent fallbackせず明示failure。

Issue #9で実装予定。

## Evaluator-backed AI

`kadoka.evaluator_ai.v1` がScript Evaluator assetを通常AI package pathへ接続する。

Runtime engineでありCreator-only helperではない。

C++ Runtimeがcanonical stateからlegal movesを生成し、各candidateを1 evaluator caseへする。

現在feature:

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

全candidateを1 batchで送る。

evaluatorは `score` を返し、optional `policy` / `confidence` は `AIInspection` へ出せる。

highest scoreをmove proposalにするが、最終legality/state transitionはGame Coreが検証する。

generated sample package:

```text
build/generated/<configuration>/evaluator_ai_manifest.json
build/generated/<configuration>/evaluator_ai_model.json
build/generated/<configuration>/in_process_evaluator.json
```

CTestはHeadlessとAI Creator analyzeの両方からこのpackageをloadする。

## Process batch protocol

`python_process` / `native_process`:

Input:

```text
KADOKA_SCRIPT_EVALUATOR 1
case 0
feature memory_bonus 2.0
feature recall_delay 0.4
end
```

Output:

```text
result 0
value score 1.6
value confidence 1.0
diag runtime=native_process
end
```

1 candidate / 1 processにせず1 batch単位で実行する。

`native_in_process` もlogical `ScriptEvaluatorCase/Result` contractは同じ。

## Benchmark

```text
kadoka_script_evaluator_probe <evaluator.json> --repeat 1000 memory_bonus=2 recall_penalty=0.4
```

比較:

```text
kadoka_script_evaluator_benchmark \
  <repeat> \
  <python-config> \
  <native-process-config> \
  <native-in-process-config>
```

Linux CIは同一inputでbenchmark値をlogへ出す。

hosted runner noiseがあるためtiming thresholdでcorrectness failureにはしない。

## Runtime / Creator

model-required Script EvaluationはRuntime。

Creatorはinspect/benchmark/tuneできるが、RuntimeからCreator Supportへ依存しない。

## Trust boundary

`native_in_process` はgame processへnative executable codeをloadする。

native pluginと同じtrust levelで扱い、untrusted packageから暗黙loadしない。

## Performance

```text
native_in_process
  -> no process startup / no temp file

native_process
  -> native executable / process isolation

python_process
  -> authoringしやすい / process + interpreter overhead

wasm
  -> 将来のportable/sandbox-oriented path
```
