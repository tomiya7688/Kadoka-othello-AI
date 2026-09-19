# Context Routing

AI支援変更で読む範囲を最小化するための索引。

列挙fileを自動的に全部読まず、該当routeから開始し、contract boundaryを跨ぐ必要がある場合だけ広げる。

## 標準順序

```text
task / changed area
  -> target source
  -> matching tests / smallest smoke
  -> direct contracts
  -> 必要な詳細doc
  -> shared/public/uncertain impact時だけbroader validation
```

## Routes

### core

board、rules、game state、pass、終局、6x6/8x8/10x10。

- Source: `src/board.cpp`, `src/rules.cpp`, `src/game.cpp`, `src/state.cpp`, `src/core_state.cpp`, 対応header
- Tests: `src/tests/core_tests.cpp`
- Docs: `doc/architecture.md`, state contract変更時は `doc/core-api.md`, `doc/data-format.md`
- Validation: rule checker -> core tests -> public Core interface変更時はRelease build

### game-record

Game Record v1、BoardState/GameAux、game_id/ply/event_index。

- Source: `src/game_record.cpp`, `src/kadoka_othello/game_record.hpp`, `src/game.cpp`
- Tests: `src/tests/game_record_tests.cpp`, Headless Record smoke
- Docs: `doc/game-record-v1.md`, `doc/data-format.md`, `doc/core-api.md`
- Validation: Game Record test -> Headless Record smoke -> shared Runtime変更ならRelease CTest

### runtime

AI invocation、package execution、Headless、Runtime-only dependency。

- Source: `src/ai.cpp`, `src/package_loader.cpp`, `src/headless.cpp`, 対応header
- Tests: `src/tests/core_tests.cpp` + bounded Headless smoke
- Docs: `doc/runtime-creator-boundary.md`, `doc/ai-protocol.md`, `doc/architecture.md`
- Validation: rule checker必須。behavior変更はfixed-seed Headless smoke。

### creator

AI Creator analyze/compare/benchmark/import。

- Source: `src/ai_creator.cpp`, `src/package_import.cpp`, `src/tools/ai_creator_main.cpp`, 対応header
- Tests: command-specific CLI/smoke。CreatorはRuntimeをlinkするため必要に応じbroader build。
- Docs: `doc/ai-creator.md`, `doc/runtime-creator-boundary.md`
- Validation: rule checker -> Creator build -> command smoke

### model-package

model root descriptor、package manifest、asset resolution、compatibility。

- Source: `src/model_descriptor.cpp`, `src/package.cpp`, `src/package_loader.cpp`, 対応header
- Tests: package/model loading smoke
- Docs: `doc/model-format.md`, `doc/ai-package.md`
- Validation: 実際のrepresentative packageをloadする。parser source確認だけで済ませない。

### script-evaluator

`kadoka.script_evaluator.v1`、Python/native/WASM/in-process evaluator。

- Source: `src/script_evaluator.cpp`, `src/kadoka_othello/script_evaluator.hpp`, `src/tools/script_evaluator_probe.cpp`
- Samples: `samples/script_evaluator/`
- Docs: `doc/script-evaluator-runtime.md`, `doc/model-format.md`, `doc/runtime-creator-boundary.md`
- Validation: bounded batch probe。compatibility変更は同一feature inputでruntime比較。
- Performance evidence: process count、temp-file I/O、per-batch latency。

### obake-kadoka

- Source: `src/obake_kadoka.cpp`, evaluator/model実装、対応header
- Assets: `src/packages/obake_kadoka/`
- Tests: fixed seed + bounded Headless。必要時のみcandidate diagnostics。
- Docs: `doc/obake-kadoka.md`
- Invariant: 明示要求なしに通常のOthello戦略知識を追加しない。

### obake-maru

- Source: `src/obake_maru.cpp`, model実装、対応header
- Assets: `src/packages/obake_maru/`
- Tests: fixed seed + bounded Headless
- Docs: `doc/obake-maru.md`
- Invariant: MaruはKadokaより弱く、記憶は極端に短く保つ。

### data-conversion

JSONL/model record、dataset conversion、format mapping。

- Source: `src/model_data.cpp`, `src/conversion.cpp`, 対応header
- Docs: `doc/model-data.md`, `doc/data-format.md`, `doc/data-conversion.md`
- Validation: disposable output + representative record
- Boundary: transformation/analysisはCreator/tooling。Runtime inference hot pathへ入れない。

### protocol

native AI contract、external process/Python transport。

- Source: `src/ai.cpp`, `src/package_loader.cpp`, `src/external_ai_session.cpp`, 対応header
- Docs: `doc/ai-protocol.md`, `doc/external-ai-protocol.md`, `doc/ai-package.md`
- Validation: producer + consumer smoke
- Invariant: Core入力へlegal-move listや旧adapter layerを再導入しない。
- Shared/public contractなのでCreator + Headlessまで広げる。

### build-policy

CMake、build script、coding rule、checker、文書方針。

- Source: `CMakeLists.txt`, `build.bat`, `tools/kadoka_rule_checker/`
- Docs: `doc/build.md`, `doc/coding-rules.md`, `doc/document-language-policy.md`
- Validation: checker direct -> normal Release build。scan対象0件でのchecker successは有効なevidenceにしない。

## Broadening rules

次の場合はselected routeより広げる。

- shared/public AI/model contract変更
- Runtime/Creator boundary変更
- Core board/rules API変更
- CMake dependency graph変更
- package/distribution内容変更
- dependency impact不明

それ以外はtargeted evidenceを優先し、acceptanceを満たしたら止める。

## 通常無視

- `build/`
- current task capsule以外の `.codex/`
- generated dataset / large JSONL
- old log / temporary evaluator file
- 無関係character model
- 無関係Issue / historical diff

## Compact completion report

```text
Changed:
- files / responsibility

Impact:
- behavior / compatibility / performance

Validated:
- command / targeted evidence + result

Unverified:
- relevant gapだけ
```
