# Kadoka Model Format

`model.json` はmodel全体そのものではなくroot descriptor。

標準package:

```text
package/
  manifest.json
  model.json
  evaluator.json
  behavior.json
  other assets...
```

- `manifest.json`: package loading / protocol情報
- `model.json`: 1 model instanceを構成するasset一覧

Root format: `kadoka.model.v1`

例:

```json
{
  "format": "kadoka.model.v1",
  "model_id": "kadoka.obake_kadoka",
  "model_kind": "character_evaluator_randomizer",
  "engine": "builtin:kadoka.obake_kadoka",
  "assets": [
    {"id":"evaluator","type":"kadoka.evaluator.linear.v1","path":"evaluator.json","required":true},
    {"id":"behavior","type":"kadoka.character_behavior.v1","path":"behavior.json","required":true}
  ]
}
```

各asset:

- stable `id`
- versioned `type`
- relative `path`
- `required`

assetはJSON限定ではない。evaluator parameter、NN、opening book、search setting、endgame table、calibration data、learned tree、executable evaluator module等を参照できる。

## Script Evaluator asset

modelは `kadoka.script_evaluator.v1` でexecutable evaluation logicを所有できる。

```json
{
  "id": "evaluator",
  "type": "kadoka.script_evaluator.v1",
  "path": "evaluator.json",
  "required": true
}
```

`evaluator.json` がroot modelと独立にruntimeを選ぶ。

```json
{"format":"kadoka.script_evaluator.v1","runtime":"python_process","script":"evaluator.py"}
```

```json
{"format":"kadoka.script_evaluator.v1","runtime":"native_process","program":"evaluator.exe"}
```

```json
{
  "format":"kadoka.script_evaluator.v1",
  "runtime":"native_in_process",
  "library":"evaluator.dll",
  "symbol":"kadoka_script_evaluator_v1"
}
```

全runtimeでlogical `features batch -> values/diagnostics` contractは同じ。

process protocol / native in-process ABIは `doc/script-evaluator-runtime.md`。

## Evaluator AI engine

Script Evaluatorでlegal candidateをscoreするpackageは次を使える。

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

packageは `interface: "native"` のまま。

C++ Runtime engineがcanonical stateからlegal candidateを生成・batch化し、evaluator assetがscoreする。

`python_process` / `native_process` / `native_in_process` を切り替えてもgame-facing AI protocolは変えない。

move validationとpackage executionはauthoritative C++ Runtime/Game側に残す。

## Complex model

```text
hybrid_ai/
  manifest.json
  model.json
  evaluator.json
  evaluator.dll
  network.onnx
  opening.bin
  search.json
  endgame.bin
```

Gameはcommon AI protocolだけを見る。model engineが理解するassetをloadする。

Obake Kadoka / Maruも同じroot formatを使うreference model。

`format` とasset `type` は独立versioningする。

- unknown optional asset: ignore可能
- unknown required asset: load failure

native in-process moduleは実行codeなのでpackage trust boundaryに含まれる。untrusted model assetを暗黙にin-process native codeへ昇格させない。
