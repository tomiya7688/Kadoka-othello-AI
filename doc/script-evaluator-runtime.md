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

実装済み。Wasmtime CLIでWASI moduleを実行する。設定は次の通り。

```json
{
  "format": "kadoka.script_evaluator.v1",
  "runtime": "wasm",
  "module": "evaluator.wasm",
  "executable": "wasmtime"
}
```

`module` はconfigからの相対pathとして解決する。`executable` はWasmtime CLIのpathまたはPATH上の名前で、省略時は `wasmtime`。
任意の `cache_config` でWasmtimeのcache用TOMLを指定できる。このpathもevaluator configからの相対pathとして解決し、moduleより前の `-C cache-config=...` として渡す。省略時はWasmtime既定のcache設定を使う。

ユーザープロファイルへ書き込めない実行環境では、例えば `"cache_config": "wasmtime-cache.toml"` を追加し、TOML内のhost側absolute pathをwritableな作業directoryへ向ける。cache directoryはWASI guestへpreopenしない。詳細は [Wasmtime Cache Configuration](https://docs.wasmtime.dev/cli-cache.html) を参照。

```toml
[cache]
directory = "E:/projects/Kadoka-othello-AI/build/wasmtime-cache"
```

WASM moduleにはbatchごとに1回だけ実行引数とし、入出力ファイルを置く一時directoryだけをWASI preopenでread/write公開する。hostの他directoryやnetworkは公開しない。process exit failure、未生成・不正なprotocol output、case IDの不一致はRuntime errorにする。

reference: `samples/script_evaluator/wasm_evaluator.cpp` と `samples/script_evaluator/wasm_evaluator.json`。

WASI SDKでsample moduleをbuildし、WasmtimeをPATHへ追加した後、probeから実行できる。

```text
<WASI-SDK>/bin/clang++ --target=wasm32-wasip1 -std=c++17 -O2 -fno-exceptions samples/script_evaluator/wasm_evaluator.cpp -o samples/script_evaluator/wasm_evaluator.wasm
kadoka_script_evaluator_probe samples/script_evaluator/wasm_evaluator.json memory_bonus=2.0 recall_delay=0.4
```

Wasmtimeが見つからない場合やmoduleがWASI commandとしてbuildされていない場合は明示failureとなり、他runtimeへfallbackしない。

### WASMのbuild・実行検証

WASMサンプルをnative sampleと比較する場合は、WASI SDKとWasmtime CLIを用意して次のようにconfigureする。通常の `build.bat` は追加toolchainを要求しない。WASM検証は既定でOFFであり、有効化時はtoolが不足していればconfigureで失敗する。

```text
cmake -S . -B build/wasm-integration -DBUILD_TESTING=ON -DKADOKA_ENABLE_WASM_TESTS=ON -DKADOKA_WASI_SDK_ROOT=<WASI-SDK> -DKADOKA_WASMTIME_EXECUTABLE=<Wasmtimeの実行file>
cmake --build build/wasm-integration --config Release
ctest --test-dir build/wasm-integration -C Release --output-on-failure
```

`generated/<構成>/wasm samples/` にWASI module、config、model、package manifestを生成する。source treeへのbinary配置は不要。`kadoka_wasm_evaluator_contract` は実際のWasmtimeを呼び、複数caseのID・数値、空batch、native/Pythonとの互換出力を検証する。native/WASMのsampleは `memory_bonus - recall_delay`、Python sampleはfeature名に応じた汎用式なので、3 runtime比較には両式が一致するinputを使い、native/WASMは負値・小数・欠落featureも別に比較する。

異常用WASI moduleを使い、process失敗、output未生成、構文不正、ID不一致、非有限数値、未終端record、余剰recordを拒否することも検証する。HeadlessとCreatorは同じ生成packageをmodel asset経由でloadする。Linux CIの `wasm-runtime` jobはWASI SDK 34.0とWasmtime 49.0.3を公式releaseからSHA-256検証付きで取得し、これらをCTestに含める。

依存toolの配布元: [WASI SDK](https://github.com/WebAssembly/wasi-sdk/releases/tag/wasi-sdk-34)、[Wasmtime](https://github.com/bytecodealliance/wasmtime/releases/tag/v49.0.3)。現状は同期process起動であり、evaluator実行時間の上限は未実装。

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
flipped_disc_count
opponent_legal_move_count
opponent_mobility
disc_difference_after_move
adjacent_to_unoccupied_corner
```

`flipped_disc_count`、`opponent_legal_move_count`、`opponent_mobility`、`disc_difference_after_move`はcandidateを着手した直後のboardから計算する。`adjacent_to_unoccupied_corner`は、着手時点で空のcornerに隣接するsquareなら1。これらは1手先の局所特徴であり、探索や終局までの価値保証ではない。特徴は追加可能で、evaluatorは未知のfeature keyを無視できる。全candidateを1 batchで送る。

native in-process reference evaluatorは、cornerを優先し、空cornerへの隣接と相手mobilityを抑える簡単な例を実装する。強さの保証ではなく、製品AIの評価はAI Leagueで行う。

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
  -> Wasmtime/WASIのfilesystem capability境界 + process isolation
```
