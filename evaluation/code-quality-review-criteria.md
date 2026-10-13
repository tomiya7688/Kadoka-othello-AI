# コード品質・学習基盤レビュー評価基準

> この文書は日本語を正本とする。
>
> 対象: Kadoka Othello AI の実装レビュー、コード品質レビュー、学習データ/学習補助基盤レビュー。
>
> 本文書は `AI_CONTEXT.md`、`doc/coding-rules.md`、各仕様書、GitHub Issue を置き換えるものではない。レビュー時の横断評価観点を固定するためのチェックリストである。

## 1. 目的

設計書との差分だけでは見落としやすい、次の問題を継続的に検出する。

- 現在は動いていても将来バグを生みやすいコード
- 境界値・異常値・状態遷移で発生する潜在バグ
- silent data corruption / silent coercion
- Runtime hot pathの不要な処理
- 大量対局・大量Dataset・relabelで支配的になるボトルネック
- 可読性・保守性・責務分離の問題
- プロジェクトのコーディングルール違反
- 学習データのjoin / provenance / label品質を壊す問題
- stateful AI、seed、rating、model identity等による再現性の問題
- 学習補助処理の不必要な全量ロード、再初期化、重複計算
- testが存在しても実際のbuild/runtime経路へ到達していない実装

## 2. レビューの優先順位

原則として次の順で見る。

1. **Correctness / Data Integrity**
2. **State / Invariant / API Boundary**
3. **Learning Data Quality / Reproducibility**
4. **Runtime / Training Pipeline Performance**
5. **Resource / Process Lifecycle**
6. **Readability / Maintainability**
7. **Coding Rules / Documentation Alignment**

性能よりrule correctness、見た目の整理よりsilent corruption防止を優先する。

## 3. Severityの目安

### P0相当

即時に修正候補とする。

- canonical game stateを壊す
- 学習label、game_id、ply、provenanceをsilentに誤記録する
- training / validation / benchmark分離を破る
- 同一IDで異なるmodel/codeを扱う
- 無期限hang、無制限memory growth、重大なresource leak
- invalid inputでstate corruptionする
- セキュリティ境界を迂回する

### P1相当

優先してIssue化する。

- 条件次第で発生する明確なバグ
- 大量自己対局/relabelで大きなbottleneckになる
- stateful AIの状態漏れなどで再現性を壊す
- NaN/Inf/overflow/partial resultを通常値として扱う
- public APIが主要invariantを簡単に迂回できる
- production相当の入力規模でmemory/I/Oが非効率

### P2相当

影響を説明できる場合にIssue化する。

- 1 file / moduleへ複数責務が集中しレビュー困難
- 重複実装により修正漏れが起きやすい
- naming / control flow / public surfaceが誤用を誘発する
- hot path外だが明確に無駄な処理がある

単なる好み、将来あるかもしれないだけの仮説、測定不能なmicro optimizationは原則Issue化しない。

## 4. Correctness / Bug Risk

### 4.1 State transition

確認する。

- `Game` / rulesだけがcanonical board transitionを所有しているか
- AI outputがproposalのまま扱われているか
- illegal moveでboard / side-to-move / plyが変化しないか
- passでboard cellsは変わらず、side-to-moveとplyが進むか
- terminal判定前後でevent/stateの順序が一貫しているか
- reset後に全mutable stateが初期化されるか
- error pathでpartial mutationが残らないか

### 4.2 Boundary values

最低限確認する。

- 0
- 1
- 最大値
- 空配列
- 重複
- unknown ID
- 欠落field
- overflow / underflow
- NaN / +Inf / -Inf
- 負値をunsignedへ渡した場合
- 極大board size / count / timeout / budget

`std::stoull`、`strtoull`、`std::stod`、`strtod` は「呼んでいるから安全」とみなさない。全文字consumed、errno、値域、finiteを確認する。

### 4.3 Collection / lifetime / mutation

確認する。

- iteration中に同じcontainerをerase/insertしていないか
- pointer/referenceがvector再allocation後も使われていないか
- public mutable containerがvalidationを迂回しないか
- callback中のunsubscribe/subscribeでiterator invalidationしないか
- move後のobjectを再利用していないか

### 4.4 Failure behavior

- malformed inputをfallback値としてsilent受理していないか
- partial outputをcomplete扱いしていないか
- EOF / timeout / child crashを正常終了扱いしていないか
- exception前にstate/file/registryの一部だけ更新されないか
- failure時に再試行して同じ副作用を二重適用しないか

## 5. Runtime / Performance

### 5.1 Hot path

特に次をhot pathとして扱う。

- `think()`
- move generation
- board evaluation
- search / Monte Carlo loop
- Headless self-play
- League game loop
- Dataset generation loop
- relabel engine execution
- exact endgame search

確認する。

- 毎手の不要なheap allocation
- rich diagnostics生成
- JSON serialize/parse
- hash map構築
- model/package reload
- process startup
- filesystem temp I/O
- full board copy
- O(N^2) lookup
- 同じ合法手/featureの重複計算

### 5.2 think / inspect境界

通常対局の `think()` はselected moveに必要な最小処理にする。

`inspect()` 用の以下を通常thinkへ混ぜない。

- 全candidate保存
- diagnostics string
- report用metadata
- Creator向けrich analysis

correctness validationは省略してよいという意味ではない。

### 5.3 Measurement

performance Issueは可能なら次を示す。

- どのloopで何回発生するか
- complexity
- allocation/process/model-load回数
- dataset rows / games / positionsが増えたときのscale
- benchmark対象
- 修正前後で守るsemantic equivalence

「速そう」だけでIssue化しない。

## 6. Readability / Maintainability

`doc/coding-rules.md` の以下を確認する。

- 1 file 1 responsibility
- 1 module 1 responsibility
- 1 function 1 coherent action
- Runtime / Creator dependency direction
- explicit data flow
- commentsはwhatではなくwhy / invariantを説明する

### Issue候補になる例

- parser、solver、JSON writer、orchestrationが1 fileに混在
- 同じJSON escape/parserが多数複製
- 100行以上のfunctionが複数のstate transitionを担当
- public APIが内部vector等を直接mutable公開
- boolの組み合わせで意味が不明
- 同じ概念が複数名で存在する
- summary文書と実装状態が矛盾

単に「長い」だけではなく、変更影響・レビュー範囲・誤用可能性を説明する。

## 7. コーディングルール

必ず `doc/coding-rules.md` とRule Checker対象を確認する。

### Dependency

許可方向:

```text
AI Creator
  -> Creator Support
  -> AI Runtime
  -> Core
```

禁止:

- Runtime -> Creator Support
- Core -> Dataset/Creator
- Headless runtimeへのCreator専用依存

### Hot path exception

低レベル実装、固定配列、直接loop等はhot pathでは許容できる。

ただし次は必要。

- benchmarkまたは明確な性能根拠
- semantic contractを維持
- dependency directionを壊さない
- optimizationを公開APIへ漏らさない

### Test

Release buildでは `assert(expr)` の式が消えるため、必要な検証に副作用を持たせない。

既存project test helper / `KADOKA_REQUIRE` 方針を守る。

### Documentation

仕様・設計・運用文書は日本語を正本とする。

実装済み機能を `current-state` が未完了扱いしていないか、逆も確認する。

## 8. 学習データ品質

### 8.1 ID / Join

特に次のjoinを確認する。

```text
game_id + ply
```

plyの定義:

- initial = 0
- accepted move = +1
- pass = +1
- illegal = 不変

`history.size()` をply代わりに使っていないか確認する。historyがpassを持たない場合は一致しない。

### 8.2 Provenance

保存・再現に必要なものが実際の推論identityを表しているか確認する。

- model ID/version
- checkpoint
- code/runtime artifact digest
- search config
- seed
- Dataset Recipe
- source Dataset
- relabel history
- exact / estimated distinction
- rating / RD

path名やmanifest versionだけで実行codeの変更を見逃していないか確認する。

### 8.3 Label validity

- selected moveは対象局面でlegalか
- candidate value/policy/Qはfiniteか
- exact labelとestimated labelを区別しているか
- partial engine resultをcomplete labelとしていないか
- disagreement計算に必要engineが揃っているか
- budget exhaustion時に通常labelを生成してよいか
- pass局面のlabel semanticsが明確か

### 8.4 Split leakage

- training
- validation
- benchmark
- league_evaluation

を混ぜない。

同一gameが別Dataset経路からtrainingへ重複流入しないか確認する。

## 9. Stateful AI / Reproducibility

AIがstatefulであること自体は問題ではない。

レビューでは「どのscopeのstateか」を確認する。

- 1 move
- 1 game
- 1 worker
- 1 batch
- 1 independent analysis position

独立局面解析でgame memoryやRNG stateを前sampleから引き継いでいないか確認する。

### 固定seed

random / character AI、sampler、schedulerは比較可能なtestでexplicit seedを持つ。

ただしseed固定だけで十分ではない。

- iteration order
- unordered container
- previous sample state
- process reuse
- model reload

でも結果が変わらないか確認する。

## 10. Confidence / Sampling

priority samplerは「何を優先するための仕組みか」を維持する。

確認する。

- low-confidenceを本当に優先するか
- disagreement / close candidateが反映されるか
- high-confidence auditが主priority枠を食い潰さないか
- audit対象にinput-order biasがないか
- `max_samples` の意味が明確か
- selected / processed / completedを混同しないか
- scale / threshold / rateがfiniteかつ妥当な値域か

## 11. Dataset / Relabel Pipeline Efficiency

### 11.1 Memory scaling

大規模JSONLで確認する。

- 全BoardStateをRAMへ載せてから少数sampleを選んでいないか
- 同じboard/candidateを複数containerへコピーしていないか
- lightweight metadataだけで先にselectionできないか
- stream / index / offset / merge joinが使えないか

目標は可能な限り

```text
memory ~= selected/active working set
```

であり、

```text
memory ~= total dataset size
```

を避ける。

### 11.2 Runtime resource reuse

- model load
- DLL/SO load
- external process startup
- network session
- evaluator initialization

をpositionごとに繰り返していないか確認する。

ただしstateful AIを単純reuseしてsample間を汚染しない。
immutable resourceとmutable inference stateを分離する。

### 11.3 Batch

evaluatorがbatch I/Fを持つ場合、

- 1 candidate 1 process
- 1 position 1 model reload

のような細かすぎる境界になっていないか確認する。

## 12. External Process / Resource Lifecycle

確認する。

- shell command文字列を組み立てていないか
- timeoutがあるか
- timeout後にkill/reapできるか
- stdout/stderr/responseサイズ上限があるか
- temp pathがparallel実行で衝突しないか
- success/error/exception/timeoutすべてでcleanupするか
- untrusted native codeを暗黙loadしないか

## 13. Issue化する条件

次の3点を原則そろえる。

1. **具体的なコード箇所がある**
2. **失敗パスまたは無駄な計算パスを説明できる**
3. **修正後のAcceptanceをtest可能な形で書ける**

Issue本文には可能な限り以下を含める。

```text
## 確認箇所
## 問題
## 影響
## 設計/規約との差分
## 対応案
## 完了条件
## 根拠
```

対応案は1実装へ固定しすぎず、守るべきinvariantを中心に書く。

## 14. Issue化しない条件

以下は原則Issueにしない。

- 既存Issueと同じ問題
- 明示的に仕様として選択されているtrade-off
- hot pathでない数要素vectorへのmicro optimization
- benchmarkなしで影響を説明できない細かい速度予想
- 単なるstyle preference
- 将来使うかもしれないだけの抽象化
- 現在のsupport範囲外を勝手に一般化する提案

重複しそうな場合は新規Issue作成前に既存Issue本文まで確認する。

## 15. レビュー手順

1. current task / Issueを読む
2. `AI_CONTEXT.md` を読む
3. `doc/coding-rules.md` と対象boundary文書を読む
4. target source + matching testsを見る
5. 既存Review Issueを検索する
6. correctness / data integrityから評価する
7. hot path / dataset scaleを評価する
8. learning join / provenance / stateful contaminationを評価する
9. concrete failure caseを作る
10. duplicateでなければIssue化する
11. IssueのAcceptanceがtest可能か確認する

## 16. レビュー完了時の確認

- [ ] bug / silent corruption
- [ ] state transition / invariant
- [ ] overflow / NaN / Inf / malformed input
- [ ] resource lifetime / timeout
- [ ] hot path allocation / serialization / process startup
- [ ] repeated model/package load
- [ ] readability / responsibility
- [ ] Runtime / Creator boundary
- [ ] 6x6 / 8x8 / 10x10
- [ ] game_id / ply / pass
- [ ] Dataset usage split
- [ ] provenance / model identity
- [ ] fixed seed / reproducibility
- [ ] stateful AI isolation
- [ ] confidence sampling bias
- [ ] partial relabel semantics
- [ ] large Dataset memory scaling
- [ ] tests / CI / build graph
- [ ] current-state / docs alignment
- [ ] existing Issueとの重複

この一覧をすべて「問題あり」にする必要はない。レビューで確認した観点を固定し、重大な見落としを減らすために使う。
