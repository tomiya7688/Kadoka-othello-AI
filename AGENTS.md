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

## 継承の共通規約（必須・レビュー基準）

### 禁止：具象クラス起点の連続継承

- **具象クラス A を継承するクラス B を、クラス C がさらに継承してはならない。** すなわち `具象 A → B → C` を禁止する。中間の B が具象か抽象かは問わない。
- **`抽象 A → 具象 B → C` は、この禁止規則の違反ではない。** ただし設計・可読性のレビューは必要とする。
- この規則単独では `具象 A → B` の一段の継承を一律には禁止しない。ただし、既存の「具象クラスの継承そのものを禁止」など、より厳しいプロジェクト規則を常に優先する。
- 言語に暗黙で存在する `Object` / `System.Object` / `java.lang.Object` 等の最上位基底型、インターフェースの継承・実装はこの違反判定の対象外とする。

### 継承を選ぶ前の設計原則

- **具体的な処理を持つクラスの継承は原則として避ける。** 抽象クラスであっても具体的な実装を持つなら同じ。継承関係そのものに意味がなく、処理の再利用だけが目的なら委譲・合成を優先する。
- 基底型と派生型に共通する意味のある形・契約が実際に存在する、または同じ形の型が今後必要になると合理的に予想できる場合に限って継承を検討する。抽象化・継承階層を「いつか使うかもしれない」というだけで作らない。
- 形式上許可される継承でも、階層が読みづらい、責務の理解が難しい、変更の波及が過大、置換可能性が成立しない等の問題があれば**レビューで不合格**とする。「禁止規則に抵触しない」は設計の承認を意味しない。

### 適用範囲と例外

- 当該プロジェクトが意図的にその継承構造を採用し、設計正本に理由・適用範囲を明記している箇所は例外とする。単に既存コードに同じ形があることを例外の根拠にしない。
- コンパイラ・解析器が入力として扱う言語の継承仕様、サンプル、検証用fixtureと、プロジェクト自身の実装規約を区別する。
- 新規・変更するプロジェクト所有コードへ適用し、無関係な既存コードの一斉書き換えを要求しない。
