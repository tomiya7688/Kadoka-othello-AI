# AI Context

このファイルは、このリポジトリでAI支援開発を始めるための小さい入口。

リポジトリ全体、全doc、全Issue、generated artifactを最初から読み込まない。

**仕様・設計・運用文書は日本語を正本とする。** 詳細は `doc/document-language-policy.md`。

## Project

- Name: Kadoka Othello AI
- Main language: C++17
- Purpose: Othello Core、AI Runtime、AI Creator、model/package tooling、Headless self-play

## 正本

- 文書言語: `doc/document-language-policy.md`
- Architecture: `doc/architecture.md`
- Runtime / Creator境界: `doc/runtime-creator-boundary.md`
- Coding rules / performance exception: `doc/coding-rules.md`
- 現在実装: `doc/current-state.md`
- 兄弟プロジェクト方針: `doc/sibling-project-alignment.md`
- Model/package: `doc/model-format.md`, `doc/ai-package.md`
- AI protocol: `doc/ai-protocol.md`, `doc/external-ai-protocol.md`
- 現在task: GitHub Issueまたは明示されたuser request
- Source/tests: `src/`, `src/tests/`

## 開始順

1. 現在task、または生成済みなら `.codex/next_issue.md` を読む。
2. `doc/context-routing.md` で最小working setを選ぶ。
3. 詳細文書を広く読む前にtarget sourceとmatching testを読む。
4. 詳細docはtaskで必要なものだけ読む。
5. Goal / Required / Acceptanceと影響boundaryが明確になったら探索を止める。

## 重要Invariant

- 強さ・最適化よりrule correctnessを先に守る。
- canonical board stateはOthello Core/Gameが所有する。AI outputはproposalであり、`Game::play()` / rules validationがauthority。
- Human / protocol / AI actionはauthoritative rule applicationを迂回しない。
- AI Runtimeはmodelを実行し、AI Creatorは作成・解析・変換・調整を行う。
- RuntimeからCreator Supportへ依存しない。
- 通常対局は軽量な `think()` pathを使い、rich inspectionは明示時だけ使う。
- random / character AIは再現可能なtest・比較のためexplicit seedを受け取れるようにする。
- Runtime hot pathでは性能上の例外を許可できるが、dependency directionは逆転させない。
- 明示的に対象外としない限り6x6 / 8x8 / 10x10を維持する。
- Obake Kadoka / Maruへ通常のオセロ知識を意図せず追加しない。
- Core APIの正は `kadoka.core_state.v1`。board / side-to-move / timeのみで、legal move listは入力に含めない。

## 兄弟プロジェクト

Kadoka Shougi AI / Kadoka Tetris AIは兄弟プロジェクト。

CI/build/release pattern、共通AI protocol、Runtime/tooling境界、benchmark、package convention、policy checkerを新設・大変更する前に、`doc/sibling-project-alignment.md` と該当兄弟実装を短く確認する。

有用な手法を採用し、ゲーム固有コードを機械的に共通化しない。

## 通常無視するもの

- `build/`
- current task capsule以外の `.codex/` output
- generated dataset / 大規模JSONL
- temporary evaluator input/output
- 無関係なIssue/doc/history
- 成功済みvalidationのfull log

## Context Priority

- P0: current task / acceptance / architecture invariant
- P1: target source / matching tests
- P2: direct dependency / contract
- P3: 詳細reference doc
- P4: history / unrelated subsystem / large generated output

## Validation

`doc/context-routing.md` のrouteに従い、最小の十分なevidenceから実行する。

共有/public boundaryを変更した場合のbaseline:

- `python tools/kadoka_rule_checker/script/kadoka_rule_checker.py .`
- `cmake -S . -B build`
- `cmake --build build --config Release`
- `ctest --test-dir build -C Release --output-on-failure`

random / character AIはfixed seed + bounded Headlessを優先する。

model/package変更ではsourceだけで推測せず、実package/assetを検証する。

CI/build/distributionではsource testとbuilt artifact smokeを別evidenceとして扱う。

## 作業ルール

- search first, read second。
- current taskと無関係なrefactorを混ぜない。
- summaryは索引であり正本の代替ではない。
- evidenceが十分なら探索を止める。
- 未確認事項は無関係箇所を読むのではなく `Unverified` と明記する。
