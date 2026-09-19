# Kadoka Othello AI

C++によるオセロCore / AI開発プロジェクト。

現在のCoreは、同じ可変盤面実装で6x6 / 8x8 / 10x10に対応する。

> このリポジトリの仕様・設計・運用文書は**日本語を正本**とする。詳細は `doc/document-language-policy.md` を参照。

## Windowsビルド

```bat
build.bat
```

CMake経由でKadoka rule checkerを実行し、ReleaseビルドとCTestまで行う。

## CI

兄弟プロジェクト共通の検証方針を採用している。

- Linux: CMake build + CTest + fixed-seed Headless smoke
- Windows: `build.bat` + fixed-seed Headless smoke + developer build artifact upload

Windows artifactは現時点では開発者向けビルドであり、正式なportable distribution packageではない。

## Headless対局

従来互換のstate JSONL出力:

```bat
build\Release\kadoka_othello_headless.exe 10000 8 dataset.jsonl 12345
```

先頭引数は `games board_size legacy_state_output seed`。

Game Record v1のBoardState/GameAuxを出力する場合:

```bat
build\Release\kadoka_othello_headless.exe 100 8 - 12345 - - board-state.jsonl game-aux.jsonl
```

## AI支援開発

リポジトリ全体を先に読み込まず、`AI_CONTEXT.md` から開始する。

```text
python tools/context_route.py --list
python tools/context_route.py script-evaluator
python tools/next_issue.py
```

- `AI_CONTEXT.md`: AI支援開発の短い入口
- `doc/current-state.md`: 現在実装の要約
- `doc/context-routing.md`: task/changeから読むsource/tests/docs/validationを選ぶ索引
- `tools/next_issue.py`: Goal / Required / Acceptanceを絞ったtask capsule生成

これらは索引であり、source/tests/Issue/詳細仕様の代替ではない。

## Coding rules

`tomiya7688/upd-commander-base-design` の適用可能な考え方を採用する。

プロジェクト固有規約とRuntime hot path例外は `doc/coding-rules.md` を参照。

C++ formatting/static-analysisの基準は、Kadoka Shougi AIと `.clang-format` / `.clang-tidy` の考え方を共有する。

## 兄弟プロジェクト

Kadoka Shougi AI / Kadoka Tetris AIは兄弟プロジェクト。

CI、検証、Context Routing、Runtime/Tooling境界など、ゲーム固有意味論やhot path性能を壊さず適用できる手法は相互導入する。

詳細は `doc/sibling-project-alignment.md`。

## ライセンス

- software / build script / 通常文書: MIT License (`LICENSE`)
- Kadoka（かどか）/ Maru（まる）のキャラクター素材: Obake Character License v1.1 (`CHARACTER_LICENSE.md`)
- その他の名前付きAIキャラクター、その設定・identity・専用asset: Kadoka AI Character License v1.0 (`AI_CHARACTER_LICENSE.md`)
- model algorithm / trained weights / datasetは、明示されている場合に個別ライセンスを持てる

## 主要文書

- `doc/document-language-policy.md`: 日本語正本ルール
- `doc/architecture.md`: 責務境界と構成
- `doc/core-api.md`: canonical JSON Core state / invalid-move event
- `doc/current-state.md`: 現在実装
- `doc/context-routing.md`: context / validation route
- `doc/sibling-project-alignment.md`: 兄弟プロジェクト間の横展開方針
- `doc/coding-rules.md`: coding / dependency / performance exception
- `doc/runtime-creator-boundary.md`: AI Runtime / AI Creator境界
- `doc/model-format.md`: model root descriptor / asset
- `doc/script-evaluator-runtime.md`: evaluator runtime / native in-process ABI
- `doc/build.md`: build / runner
- `doc/data-format.md`: canonical state / Dataset境界
- `doc/game-record-v1.md`: BoardState + GameAux JSONL
- `doc/dataset-pool.md`: Dataset Registry / Recipe / provenance / 重複排除

C++ source/headerは `src/` 配下へ置く。
