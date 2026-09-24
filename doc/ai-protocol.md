# AI Protocol

## 目的

全AIは同じ意味論のstateを受け取り、move proposalを返す。

Game Coreが常にauthority。

## Input

`AIInput` は `kadoka.core_state.v1` と意味論的に同一なnative typed view。

含むもの:

- current board
- side to move
- time

legal-move listは含まない。

native engineはzero-copy board referenceを受け取る。合法手が必要なengineはboard + side-to-moveから内部生成する。

例:

- Random AI: legal setを生成してsampleする。
- Evaluator AI: legal candidateを生成し、feature batchを作ってscoreする。
- Obake Kadoka / Maru: legalityを知らず、空きマスを候補として見る。

## Output

`AIOutput` はboard position 1つのproposal。

proposalは `Game::play()` へ渡し、native/package AI由来でも合法とは信用しない。

## Invalid move

illegal proposal:

- board不変
- side-to-move不変
- ply不変
- `GameEventType::InvalidMove` 発行

Headlessは `HeadlessConfig::max_invalid_attempts_per_turn` まで同じstateで再問い合わせできる。

GUI / character behavior / loggerはCore legality logicへ混ざらずevent購読で反応できる。

## Package Runtime

`AIPackage` は `IAIEngine` を保持する。

旧 `PassThroughAdapter` / `DropLegalMovesAdapter` は撤去済み。legal moves自体がCore inputではないため情報をdropするadapterも不要。

historical manifestに旧 `adapter` keyが残っていても未知compatibility fieldとして無視できる。新manifestでは出力しない。

## External / script

external/script backendはcanonical JSON stateを受け取る。

transport framing、process lifetime、timeoutはRuntime責務でありCore semanticsを変更しない。

## Performance rule

JSONがAPI上の正であることと、native hot pathで毎回serializeすることは別。

- public semantic contract: `kadoka.core_state.v1`
- native execution: `CoreStateView`
- external transport: `core_state_to_json()`

bitboard、fixed buffer、SIMD layout、tensor等を内部派生してよいが、別のpublic Core APIにしない。


## Standard Runtime metrics

`AIOutput` はmove proposalに加えてoptionalな軽量metricsを持てる。

- `nodes`
- `simulations`
- `depth`
- `search_effort`

metricsはGame ruleやCore stateではない。

Headless/Leagueが `collect_metrics=true` の場合だけ集計し、未報告AIはreport count 0として区別する。

external/script backendは次のdiagnostic keyを返すとstandard metricsへ変換される。

```text
diag nodes=12345
diag simulations=800
diag depth=7
diag search_effort=1.5
```

unknown diagnosticは通常の `AIInspection::diagnostics` として保持できる。
