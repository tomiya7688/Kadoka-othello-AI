# AI Runtime / AI Creator境界

## 目的

Kadoka Othello AIでは、AI実行経路とAI作成・開発経路を分離する。

これは整理上だけでなくperformance rule。

Game / Headless self-playは大量のinferenceを実行するため、開発専用analysis、conversion、import、rich diagnostics、dataset toolingをhot path依存へ入れない。

## AI Runtime

packaged modelを読み込み、対局中にできるだけ小さいoverheadでmove proposalを返す。

Runtime責務:

- `manifest.json` load
- root `model.json` load
- required asset解決
- canonical typed Core stateをmodelへ渡す
- native/model/script inference
- character memory等のmodel-local state
- selected move proposal
- 明示時だけlightweight inspection
- Headless self-play
- Game Recordのminimal raw recording

Runtimeから依存禁止:

- AI Creator batch analysis
- dataset conversion
- import/package creation
- format conversion table
- training/tuning orchestration
- rich report
- Creator-only benchmark orchestration

## AI Creator

Runtime上に構築する開発application。

Creator責務:

- model作成/package化
- external AI import
- candidate value / diagnostics inspection
- 複数AI比較
- batch position analysis
- benchmark
- model/dataset format conversion
- package asset validation
- tuning
- dataset / analysis record export

Creator -> Runtime依存は許可する。

Runtime -> Creatorは禁止。

## Build dependency

```text
kadoka_othello_runtime
        ^
        |
kadoka_othello_creator_support
        ^
        |
kadoka_othello_ai_creator
```

Game/Headlessは `kadoka_othello_runtime` のみlinkする。

CreatorはCreator Support + Runtimeをlinkする。

便利だからという理由で再統合しない。

## Shared contract

共有するのはCreator実装ではなくcontract。

- `AIPackageManifest`
- `ModelRootDescriptor`
- `ModelAssetDescriptor`
- `IAIEngine`
- `AIInput`
- `AIOutput`
- optional `AIInspection`
- `ScriptEvaluator`

Game-facing contract:

```text
input:
  board
  side to move
  time

output:
  selected move proposal
```

public semantic source of truthは `kadoka.core_state.v1` JSON。

native Runtimeは同じ意味論の `CoreStateView` を直接使い、毎inferenceのJSON serializationを避ける。

legal movesが必要なengine/toolingはboard + side-to-moveから派生する。Core input fieldではない。

## Hot path

通常対局:

```text
Game
  -> Runtime package
  -> model engine
  -> think()
  -> move proposal
  -> Game validation
```

開発tool:

```text
AI Creator
  -> Runtime package
  -> model engine
  -> inspect()
  -> move + candidates + diagnostics
```

`think()` からCreator analysisを呼ばない。

model inferenceそのものに必要でないdataset record/string/report/conversion objectを通常inference pathで生成しない。

## Script Evaluator

`kadoka.script_evaluator.v1` はmodelがinferenceにscript evaluatorを必要とできるためRuntime機能。

Runtimeがmodel-owned evaluatorを実行してもCreator機能にはならない。

```text
model-required computation -> Runtime
model-development tooling   -> Creator
```

`python_process` ではcandidateごとのprocess起動を禁止し、可能な限り1 batch / 1 invocationにする。

## Dataset / Record

Headless self-playとminimal Game Record生成はRuntimeに属する。

重いserialization変換、relabel、format conversion、analysis、training dataset構築はCreator/tooling。

```text
Runtime self-play
  -> Game Record / raw stream
  -> Creator / Dataset tooling
  -> analysis / conversion / training
```

## Dependency review

新component追加時:

1. 対局中のmove選択に必要か。
   - yes: Runtime candidate
   - no: Creator/toolingを検討
2. report/dataset conversion/comparisonを作るか。
   - Creator/tooling
3. inferenceに必要なmodel asset interpreterか。
   - Runtime
4. model development/tuning中だけ必要か。
   - Creator/tooling
5. 毎inferenceでallocation/serialization/process launch/dependency sizeを増やすか。
   - 厳密に必要でなければRuntimeへ入れない

## Invariant

**AI CreatorはAIを作成・解析・調整する。AI RuntimeはAIを実行する。**

この境界はperformance requirement。
