# Kadoka Othello AI — Codex Context

**仕様・設計・運用文書は日本語を正本とする。** `doc/document-language-policy.md` を参照。

## 目的

C++17によるOthello engine / AI開発プロジェクト。

Headless実行、model/package、evaluator integration、AI Creator toolingを含む。

## 標準workflow

- `start_task.bat`: 優先度の高いOpen Issueを選び、`.codex/next_issue.md` へ小さいcontextを書き出す。
- `finish_task.bat`: `build.bat` 経由で既存CMake Release build + CTestを実行する。
- `finish_pr.bat`: `pull_request.bat` 経由でcompact PR workflowを実行する。
- 旧 `next_issue.bat` / `pull_request.bat` もdirect entrypointとして有効。

## Build / Test

- Windowsでは `build.bat`。
- CMake configure → Release build → CTestを行う。
- Issueで明示されない限り、既存CMake/CTestを別test frameworkへ置き換えない。
- Release testで副作用付き `assert(expr)` を使わない。Releaseでは式自体が消えるため、testは `KADOKA_REQUIRE` 等の常時実行checkを使う。

## Architecture rules

- Runtime codeは `kadoka_othello_runtime` に置き、Creator-only toolingから独立させる。
- Creator Support -> Runtime依存は許可する。Runtime -> Creator Supportは禁止。
- taskが明示しない限りC++ source/headerは `src/` 配下。
- Issueが明示変更しない限り6x6 / 8x8 / 10x10対応を維持する。
- hidden behaviorよりdeterministic / testable changeを優先する。
- Core APIはboard / side-to-move / timeのみ。legal move listを再導入しない。
- AI outputはproposal。Game Coreが合法性と状態遷移をauthorityとして処理する。

## Context discipline

- `.codex/next_issue.md` がある場合、このファイルの次に読む。
- 原則として `.codex/next_issue.md` が指定する `doc/*.md` と実装に直接必要なfileだけ読む。
- 全 `doc/` や全Issueを毎回preloadしない。ただしIssue間priority/dependencyの確認がtaskそのものならOpen Issue一覧を確認してよい。
- routine PR preparationでfull diffを先に読まない。changed filenames、diff stat、commit summary、test resultを優先し、問題診断時だけfull diffを読む。

## 主要文書

- `doc/document-language-policy.md`: 文書言語・正本
- `doc/architecture.md`: 責務境界
- `doc/build.md`: build / runner
- `doc/runtime-creator-boundary.md`: Runtime / Creator依存
- `doc/ai-creator.md`: AI Creator
- `doc/ai-package.md`: AI package
- `doc/ai-protocol.md`, `doc/external-ai-protocol.md`: AI protocol
- `doc/data-format.md`, `doc/data-conversion.md`: game data / conversion
- `doc/model-format.md`, `doc/model-data.md`: model representation
- `doc/obake-kadoka.md`, `doc/obake-maru.md`: built-in character AI
