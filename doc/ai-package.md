# AI Package仕様

## 目的

built-in / external AIを共通package contractで扱う。

Game-facing入力:

- current board
- side to move
- time

legal move listは含まない。

出力:

- selected move proposal

development toolは追加でinspection dataを要求できる。

## 標準package

```text
my_ai/
  manifest.json
  metadata.json
  model.json | model.bin | model-specific files
  optional runtime files
  optional assets/
```

- `manifest.json`: package entry point
- `model`: separate model dataが不要なengineではoptional
- `metadata`: legacy/internalではoptional、新規distribution modelでは推奨

例:

```json
{
  "id": "kadoka.obake_kadoka",
  "name": "Obake Kadoka",
  "version": "0.1.0",
  "protocol_version": 1,
  "board_sizes": [6, 8, 10],
  "interface": "native",
  "entry": "builtin:kadoka.obake_kadoka",
  "model": "model.json",
  "metadata": "metadata.json",
  "capabilities": ["move", "inspection", "dataset_generation"]
}
```

## Required manifest fields

- `id`
- `name`
- `version`
- `protocol_version`
- `board_sizes`
- `interface`
- `entry`

Optional:

- `model`
- `metadata`
- `capabilities`
- external/script用 `transport`, `executable`, `timeout_ms`

旧manifestの `adapter` keyはcompatibility上unknown fieldとして無視可能だが、現行manifest fieldではない。

## Model files

`model.json` はOthello model root descriptor。

evaluator、behavior、network、opening、search、script evaluator等をassetとして参照できる。

```text
manifest -> engine selection / runtime entry
manifest.model -> executable model/config/assets
manifest.metadata -> family identity/provenance/requirements/benchmark metadata
```

`metadata.json` は `kadoka.ai_metadata.v1`。詳細は `doc/family-model-metadata.md`。

## Interface

- `native`: in-process C++
- `dynamic_library`: separately distributed native engine用に予約
- `external_process`: executable persistent/legacy process
- `python`: Python script/process compatibility interface
- `network`: remote inference用に予約

high-throughput Dataset生成は `native` / 将来の `dynamic_library` を優先する。

## Core APIとの関係

packageへ渡るsemantic stateの正は `kadoka.core_state.v1`。

nativeは同値な `CoreStateView` を使い、external/scriptはcanonical JSONへserializeする。

合法手が必要なAIはboard + side-to-moveから内部生成する。

旧 `PassThroughAdapter` / `DropLegalMovesAdapter` は撤去済み。

## Obake Kadoka例

```text
src/packages/obake_kadoka/
  manifest.json
  metadata.json
  model.json
  evaluator.json
  behavior.json
```

Obake Kadokaはnative modelなので、Dataset generationでper-move process startup / JSON serialization / temp-file I/Oを必要としない。
