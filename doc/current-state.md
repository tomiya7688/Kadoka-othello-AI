# 現在実装

この文書は現在の実装状態を短く確認するための索引。sourceや詳細仕様の代替ではない。

## 実装済み

### Othello Core / Runtime

- 6x6 / 8x8 / 10x10を同じ可変盤面実装で扱う。
- rules、合法手生成、game state、pass、終局判定、Headless実行。
- canonical Core API `kadoka.core_state.v1` は board + side-to-move + time のみ。
- native hot pathは同じ意味論のzero-copy `CoreStateView` を使う。legal move listはCore/AI入力fieldではない。
- `Game` は `MoveAccepted` / `InvalidMove` / `Pass` / `Terminal` eventを発行する。
- 違法手ではboard / side / plyを変更しない。
- AI package manifest loading。
- RuntimeとCreator Supportは別CMake target。
- `KADOKA_BUILD_AI_CREATOR=OFF` でCreator非依存のRuntime-only build/testが可能。
- HeadlessはRuntimeのみをlinkする。
- AI outputはproposalであり、`Game` / rulesがcanonical state transitionを所有する。

### Game Record v1

- `kadoka.board_state` v1 JSONL。
- `kadoka.game_aux` v1 JSONL。
- ULID `game_id`、`ply`、`event_index`。
- initial / accepted move / pass / illegal / invalid notification / terminalを記録可能。
- BoardStateとGameAuxは `game_id + ply` でjoin可能。
- writer、single-game reader、multi-game readerを実装。
- Headlessから2ストリームを出力可能。
- 6x6 / 8x8 / 10x10、pass、illegal、terminal、unknown optional fieldをCTestで検証。

### Model / Package

- `manifest.json` がAI packageを記述する。
- `model.json` はroot model descriptorで、複数assetを参照可能。
- assetはevaluator、behavior、network/search/opening data等を表現可能。
- `ModelRootDescriptor` / `ModelAssetDescriptor` がassetを解決する。

### Script Evaluator

- `kadoka.script_evaluator.v1` はRuntime機能。
- `python_process` 実装済み。
- `native_process` 実装済み。
- `native_in_process` はstable C ABIのDLL/SO pathとして実装済み。
- native in-processのoutputはhost-owned callbackで返し、C++ containerをmodule ABI越しに渡さない。
- 全runtimeでbatch inputを使用する。
- `wasm` は予約済みだが未実装。
- CMakeでnative in-process sample moduleとrunnable configを生成する。
- CTestにnative in-process smokeがある。
- evaluator probeはrepeat実行に対応する。

### Character AI

#### Obake Kadoka

- Coreからlegal movesを受け取らず、空きマスを自分で評価する。
- 通常のオセロ戦略ではなくKadoka固有のlocal board featureを使う。
- weighted randomnessを使う。
- 推定されたrejectを含む短期attempt memoryを持つ。
- evaluator / behavior parameterはmodel assetで調整可能。

#### Obake Maru

- Coreからlegal movesを受け取らず、空きマスを自分で評価する。
- Kadokaより弱いlocal-interest evaluator + 高randomness。
- 直前のrejectだけ覚える。

### Dataset Pool / Recipe

- `kadoka.dataset_entry.v1` Dataset Registry JSONL。
- provenance: source type / board size / generator/opponent model version / parent / search config / relabel history / created_at / license / game IDs。
- 6x6 / 8x8 / 10x10を明示区別。
- `training` / `validation` / `benchmark` / `league_evaluation` usage分離。
- `derive_dataset()` でparent game subsetからvirtual derived Datasetを作成可能。
- parent存在 / board size / game subset / cycleをRegistry validation。
- `kadoka.dataset_recipe.v1` でmodelごとのDataset比率を設定可能。
- Recipe resolve時に複数Dataset間の同一 `game_id` を重複排除。
- `training_recipe` を `kadoka.ai_metadata.v1` へ埋め込み可能。
- `kadoka_dataset_tool` でvalidate / plan / attach-recipe。
- Dataset PoolはCreator Support側にありRuntime-only buildへ依存しない。

### AI Creator / Tooling

- analyze / batch analyze / compare / benchmark。
- package import、data/model conversion。
- `AI_CONTEXT.md` がAI支援開発のcompact entrypoint。
- `doc/context-routing.md` がtask categoryからsource/tests/docs/validationへrouteする。
- `tools/context_route.py` が必要routeだけ表示する。
- `tools/next_issue.py` がGoal / Required / Acceptanceを絞ったtask capsuleを生成する。
- `tools/kadoka_rule_checker/` が機械的に判定できるproject ruleを検査する。
- checkerは通常CMake buildでRuntime compile前に実行される。

### Quality / CI

- 仕様・設計・運用文書は日本語を正本とする。詳細は `doc/document-language-policy.md`。
- README / AI_CONTEXT / AGENTS / PR template / sibling policyの日本語正本markerをRule Checker `KAD103` で確認する。
- PR templateに日本語正本更新・Core API境界・Release testのchecklistを持つ。
- `.clang-format` / `.clang-tidy` の基準をKadoka Shougi AIと共有しつつC++17へ合わせる。
- Linux CI: build / CTest / fixed-seed Headless smoke / Creator-disabled Runtime-only validation。
- Windows CI: `build.bat` / fixed-seed Headless smoke / developer artifact upload。
- Release buildでは `assert(expr)` の式自体が消えるため、testは常時評価される `KADOKA_REQUIRE` を使用する。
- sibling cross-adoptionは `doc/sibling-project-alignment.md` に記録する。

## Open / Pending

- production character AIが `native_in_process` Script Evaluator assetを実際に必要とする構成への接続。
- `python_process` / `native_process` / `native_in_process` の同一入力比較performance measurement。
- WASM Script Evaluator runtime。
- Windows developer artifactを正式distributionと呼ぶ前のportable distribution境界定義。
- false positiveが少ない範囲でのrule checker追加。
- Issueで計画されているその他AI family / Dataset / relabel / training基盤。

## Performance-sensitive boundary

通常pathへ不要なallocation、serialization、process launch、rich diagnosticsを追加しない。

- `think()`
- move generation
- board evaluation
- search / Monte Carlo loop
- Headless self-play
- dataset-generation execution loop

Creator-onlyの解析・変換はこれらhot pathの外に置く。

## Validation baseline

広い/shared change:

```text
python tools/kadoka_rule_checker/script/kadoka_rule_checker.py .
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

局所変更では `doc/context-routing.md` のtargeted evidenceを先に実行し、public/shared contract変更または影響不明時だけ広げる。
