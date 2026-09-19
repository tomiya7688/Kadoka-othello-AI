# Coding Rules

`tomiya7688/upd-commander-base-design` の適用可能な原則をKadoka Othello AI向けに採用する。

目的はmaintainabilityを保ちつつ、Runtime hot pathへ不要なperformance bottleneckを入れないこと。

## 1. 1 file 1 responsibility

source/header pairは原則1つの主責務を持つ。

例:

- `obake_kadoka_evaluator.*`: evaluator
- `obake_kadoka_model.*`: model loading
- `script_evaluator.*`: Script Evaluator runtime
- `package_loader.*`: package/runtime loading

Creator-only analysis/conversionをRuntime fileへ混ぜない。

小さいhelper type/private helperは支える責務と同居してよい。

## 2. 1 module 1 responsibility

moduleはclass/namespace/free-function groupいずれでもよい。OOP必須ではない。

class境界より責務境界を優先する。

## 3. 1 function 1 coherent action

parsing / validation / evaluation / persistence / presentation等、無関係な処理は分ける。

ただしhot pathを細分化しすぎてcall/abstraction overheadを作らない。

## 4. Commentは意図を書く

processing block、invariant、非自明optimization、boundary ruleの理由を書く。

次行のsyntaxを言い換えるだけのcommentは避ける。

非自明なperformance exceptionは理由を残す。

## 5. Closed processing module

processing moduleはowner/coordinatorから呼ばれ、resultを返す形を基本とする。

```text
Game / Headless / AI Runtime coordinator
  -> processing module
  -> result
```

独立processing module間のhidden horizontal call chainを避ける。

許可:

- 同一責務private helper
- pure function
- data/value type
- 明示shared utility

## 6. Dependency direction

```text
AI Creator
    -> Creator Support
    -> AI Runtime
    -> Othello Core
```

逆方向は禁止。

- RuntimeからCreator Supportをinclude/linkしない。
- Headless/GameからDataset conversion/import/analysisへ依存しない。
- CreatorはRuntimeを利用できる。
- shared contractへCreator-only / GUI-only behaviorを入れない。

include/import/referenceにも適用する。

## 7. Runtime hot-path exception

実測または明確なcomplexityから通常分割がbottleneckになる場合、性能優先の局所例外を許可する。

代表hot path:

- move generation
- board access
- evaluation
- search
- transposition table
- rollout / Monte Carlo
- large-scale self-play
- character candidate evaluation
- model inference

許可例:

- fixed-size array
- tightly-coupled stageのloop統合
- inner loop virtual call回避
- small helper inline
- data-oriented structure
- cache/precompute
- board-size specialization
- 8x8 bitboard specialization

条件:

1. performance-sensitive範囲に限定。
2. public responsibility boundaryは理解可能。
3. 非自明なら理由記録。
4. 可能ならbenchmark/measurable requirementで根拠を持つ。
5. layer dependencyを逆転しない。

## 8. External dependency containment

外部dependencyは必要module内へ閉じ込める。

例:

- Python process -> Script Evaluator runtime
- WASM -> WASM evaluator module
- GUI framework -> GUI layer

common AI contractへlibrary-specific typeを漏らさない。

## 9. Build / Quality

通常変更:

- compile成功
- existing tests成功
- CI維持
- unresolved warning/errorを増やさない
- 重要behaviorへtest追加

formatter/linter suppressionは狭く理由を持つ。

Release testでは副作用付き `assert(expr)` を禁止する。Releaseでは `NDEBUG` により式自体が消えるため、常時実行される `KADOKA_REQUIRE` 等を使う。

## 10. 文書

仕様・設計・運用文書は日本語を正本とする。

詳細: `doc/document-language-policy.md`

仕様変更は日本語正本を先に更新する。

## Review checklist

- file/moduleの主責務は1つか。
- functionはcoherent actionか。
- RuntimeはCreator Supportから独立しているか。
- 不要なhorizontal couplingはないか。
- commentは意図を説明しているか。
- hot pathへallocation/copy/process launch/virtual dispatchを不必要に追加していないか。
- performance exceptionの範囲/理由は明確か。
- Core APIへlegal moves等の派生情報を戻していないか。
- 日本語正本が更新されているか。
- Release testで副作用付きassertを使っていないか。
- build/testは通るか。

## 出典

`upd-commander-base-design` のrecommended practice、dependency rule、implementation quality requirementを基に、Othello Runtimeのperformance exceptionを加えている。
