# 兄弟プロジェクト連携方針

Kadoka Othello AI / Kadoka Shougi AI / Kadoka Tetris AIは兄弟プロジェクト。

ゲーム固有実装を最低共通分母へ押し込まず、実証済みの開発手法・境界・AI研究基盤を相互利用する。

- `tomiya7688/Kadoka-othello-AI`
- `tomiya7688/Kadoka-shougi-ai`
- `tomiya7688/kadoka_tetris_ai`

## 文書言語

Othello側の仕様・設計・運用文書は**日本語を正本**とする。

兄弟から英語文書・実装方針を取り込む場合も、Othello側へ採用した仕様は日本語正本へ記録する。

詳細: `doc/document-language-policy.md`

## 共通Invariant

### Authoritative Core

AI outputはproposalでありauthoritative stateではない。

Game/rules Coreがproposalを検証し、canonical state transitionを所有する。

character AIが意図的に弱い/illegal actionを提案してもrule validationを迂回しない。

### Runtimeを小さく保つ

match-time Runtimeにはgame/inference実行に必要なものだけ置く。

training、model creation、conversion、rich analysis、report、大規模Dataset processingはCreator/tooling。

Runtimeから上位toolingへ依存しない。

### Correctness before strength

search/model strengthよりrule correctnessとdeterministic reproductionを先に守る。

canonical stateを壊す、またはdebug再現できない高速AIを成功とみなさない。

### Deterministic seam

random behaviorは可能な限りexplicit seedを受ける。

benchmark/AI比較はfixed input/seed + bounded runtimeを使う。

### Semantic action boundary

human/AIはいずれも最終的に同じauthoritative game-action validationへ到達する。

UI/protocol transportを第2 rules engineにしない。

### Hot-path flexibility

責務分離がdefault。

ただしmove generation、search、evaluation、rollout等でabstraction overheadが実測上問題なら局所最適化を許可する。

performance exceptionでもlayer dependency directionは壊さない。

## 共通AI backend vocabulary

概念名を可能な範囲で共有する。

- `native`
- `dynamic_library`
- `external_process`
- `script`
- `network`

game state/action typeは各ゲーム固有。

backendはaction/resultをproposalし、authoritative Game Coreが適用する。

Othelloのpackage interface `python` は互換名で、概念上はshared `script` category。

external/process/script transportをCoreへ入れない。

high-throughput pathはper-move process startよりpersistent / in-process / nativeを優先する。

## 兄弟から採用したもの

### Kadoka Shougi AI

- correctness-before-strength
- engine resultはCore validationまでnon-authoritative
- C++ `.clang-format` / `.clang-tidy`
- narrow engine/runtime/core dependency
- native/process/script/network等のbackend vocabulary
- BoardState / GameAux分離型Game Record

### Kadoka Tetris AI

- deterministic seed/input
- Headless-first test
- source CIとWindows artifact validationを別evidenceとする
- one-command build
- 実施していないGUI/artifact検証を成功扱いしない

### Kadoka Othello AI

- Runtime / Creator分離
- context reduction / task routing
- compact mechanical rule checker
- model/package assetとexecution toolingの分離
- canonical JSON stateとnative typed hot pathの意味論統一

## Cross-project adoption

project-wide workflow、CI、checker、model/package convention、major runtime boundaryを追加/変更する前に兄弟実装を短く確認する。

採用条件:

1. 同じ問題が存在する。
2. このゲームのsemantics/performanceを維持できる。
3. maintenance costが妥当。
4. このrepositoryでlocal validationできる。

language/game-specific detailを機械的にcopyしない。

## CI baseline

```text
static / policy check
  -> source build / unit test
  -> deterministic / Headless smoke
  -> distribution影響時はplatform/artifact build
```

OthelloはLinux CMake build/testとWindows developer artifact CIを持つ。

portable distribution境界を定義・smoke testするまではWindows artifactを正式distributionと呼ばない。

## Review trigger

次を変更するとき兄弟repositoryを再確認する。

- CI/build/release
- common AI/engine protocol
- Runtime/Creator、runtime/tooling boundary
- deterministic benchmark method
- package/model format
- dependency checker / coding-policy automation
- distribution/artifact validation
- Dataset / provenance / Champion-Candidate等の共通研究基盤
- 文書運用のようなproject-wide policy

目標はshared learningであり、実装を強制的に同一化することではない。
