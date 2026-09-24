# AI League

AI LeagueはAI Runtime / Headlessの上にあるorchestration層。

rating、scheduler、世代管理、League logを `kadoka_othello_runtime` へ混ぜず、`kadoka_othello_league_support` として分離する。

## Core API境界

League参加AIも通常対局と同じ境界を使う。

```text
kadoka.core_state.v1
  board
  side-to-move
  time
      ↓
AI proposal
      ↓
Game authoritative validation
```

legal move listはCore入力ではない。

illegal proposalではboard / side / ply不変 + `InvalidMove` event。

## Participant identity

League participantはAI表示名だけでは識別しない。

`LeagueParticipantConfig`:

- package manifest path
- board size
- deterministic participant seed
- package assetでまだ表現していない設定用 `config_hash`

participant IDは次をfingerprintする。

- package ID / version
- package interface / entry
- manifest内容
- model descriptor内容
- model asset内容
- board size
- seed
- config hash

例:

```text
kadoka.obake_kadoka@0.1.0:b8:s12345:h0123456789abcdef
```

同じAI名でもcheckpoint/configが異なれば別競技個体。

## Persistent League Registry

schema:

```text
kadoka.league_registry.v1
```

Registryはparticipant設定とratingをJSONLで永続化する。

保存:

- participant_id
- manifest path
- board size
- seed
- config hash
- role
- checkpoint ID
- rating / RD
- games / wins / losses / draws

Registry load時に現在のpackage/configからparticipant fingerprintを再計算し、保存済みparticipant_idと一致しなければrejectする。

これにより同じpathのmodel内容だけ差し替えて過去ratingを誤継承することを防ぐ。

### Role

- `standard`
- `champion`
- `candidate`
- `hall_of_fame`

旧世代/checkpointは通常packageとして登録し、`hall_of_fame` + optional `checkpoint_id` を持たせられる。

## Scheduler

schedulerは対局pairを決めるだけで、Game ruleやAI inferenceを持たない。

### round_robin

同じboard sizeの全participant pair。

### random

全eligible pairをfixed seedでshuffleし、指定pair数を生成する。

pair候補数より多い場合は再shuffleして繰り返す。

### rating_band

rating差が `max_rating_gap` 以下のpairだけからfixed seedで選ぶ。

近いstrength帯の対局生成用。

### candidate_champion

同じboard sizeのCandidateとChampionを組み合わせる。

Champion/Candidate昇格基盤の評価戦に利用する。

### candidate_hof

CandidateとHall of Fameを組み合わせる。

新modelが現Championだけに過適応していないか、旧世代に対する回帰を確認できる。

## 色交換

1 scheduled pairは `games_per_color=1` の場合、

```text
A black vs B white
B black vs A white
```

を必ず実行する。

色交換後は両AI packageをreloadし、character memory等のmatch-local stateを次gameへ漏らさない。

## Seed / reproducibility

各gameのAI seedは次からdeterministicに導出する。

- League seed
- participant seed
- game index
- color

`LeagueGameRecord` は2種類のIDを持つ。

### game_id

26文字ULID。

Game Record v1のBoardState/GameAuxと同じIDを使う。

### reproducibility_key

participant/config/League seed/game indexから決定的に作る `run-...` key。

game_idはprovenance join用、reproducibility_keyは同条件再実行・duplicate run分析用。

## Rating

online Glicko-1 style。

- initial rating: 1500
- initial RD: 350
- RDをrating uncertaintyとして保持
- win/loss/drawを保持
- Registryへ保存して次runから継続

board sizeはparticipant identityに含まれ、schedulerもboard sizeを明示するため6x6 / 8x8 / 10x10 ratingが混在しない。

## Runtime metrics

`AIOutput::metrics` はoptional lightweight metricsを持てる。

- `nodes`
- `simulations`
- `depth`
- `search_effort`

Headlessは `collect_metrics=true` の場合だけ集計する。

League logには:

- AI call count
- total/max think time
- invalid attempt
- total nodes + report count
- total simulations + report count
- max depth + report count
- total search_effort + report count

を保存する。

metricを提供しないAIではreport countが0なので、「0 nodes探索」と「未報告」を区別できる。

external/script AIはdiagnostic:

```text
diag nodes=...
diag simulations=...
diag depth=...
diag search_effort=...
```

を返すとstandard Runtime metricsへ変換される。

## Game log

```text
format = kadoka.league_game.v1
```

保存:

- game_id
- reproducibility_key
- black/white participant ID
- board size
- actual AI seed
- outcome
- elapsed game time
- Runtime metrics
- rating/RD before/after

## 学習データ

### 推奨: Game Record v1

Registry-runではBoardState / GameAuxの2ストリームを出力できる。

```text
kadoka.board_state v1
kadoka.game_aux v1
```

League game logとGame Recordは同じ `game_id` を共有する。

このためDataset Pool側で、

```text
League provenance
    + game_id
    + BoardState/GameAux
```

をjoinできる。

弱いAI、Obake、old checkpointも局面生成源として保存できるが、その着手をtrusted training labelとは扱わない。

後段のMulti-engine Relabelingで再評価する。

### legacy position stream

旧 `kadoka.league_position.v1` はcompatibility用として残す。

新しいDataset pipelineはGame Record v1 + Dataset Poolを優先する。

## API

- `run_round_robin_league()`: 旧互換wrapper
- `run_league_schedule()`: 任意pairing実行
- `LeagueRegistry`: participant/rating永続化
- `schedule_round_robin()`
- `schedule_random_matches()`
- `schedule_rating_band()`
- `schedule_candidate_champion()`
- `schedule_candidate_hall_of_fame()`
- `run_registry_schedule()`

## CLI

### 旧round-robin

```text
kadoka_ai_league <board-size> <games-per-color> <seed> \
  <game-log.jsonl|-> <position-dataset.jsonl|-> \
  <manifest.json> <manifest.json> [...]
```

### Registryへ追加

```text
kadoka_ai_league registry-add <registry.jsonl> \
  <standard|champion|candidate|hall_of_fame> \
  <board-size> <seed> <config-hash|-> <checkpoint-id|-> \
  <manifest.json>
```

### Registry表

```text
kadoka_ai_league registry-table <registry.jsonl>
```

### Registry scheduler実行

```text
kadoka_ai_league registry-run <registry.jsonl> \
  <round_robin|random|rating_band|candidate_champion|candidate_hof> \
  <board-size> <games-per-color> <seed> \
  <game-log.jsonl|-> <board-state.jsonl|-> <game-aux.jsonl|-> \
  [pair-count] [max-rating-gap]
```

run完了後、更新rating/RDをRegistryへ保存する。

## 境界

AI LeagueはGame Coreの代わりにrule validationをしない。

Dataset Pool / training recipeもLeague Supportへ逆依存させない。

```text
AI League
  -> Headless / Runtime
  -> Game Core

Game Record
  -> Dataset Pool / Creator
```

## 設計原則

**強いAIだけを集めるのではなく、異なる思想・強さ・探索分布・世代を同じ競技基盤へ載せる。**
