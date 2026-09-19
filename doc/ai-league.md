# AI League

AI LeagueはAI Runtime / Headlessの上にあるorchestration層。

`kadoka_othello_runtime` へrating、tournament scheduling、league loggingを混ぜず、`kadoka_othello_league_support` として分離する。

## Participant identity

league participantはAI表示名だけでは識別しない。

`LeagueParticipantConfig`:

- package manifest path
- board size
- deterministic participant seed
- package assetでまだ表せない設定用のoptional `config_hash`

participant IDは次もfingerprintする。

- package ID / version
- package interface / entry
- manifest content
- model descriptor content
- model asset content
- board size
- seed
- extra config hash

例:

```text
kadoka.obake_kadoka@0.1.0:b8:s12345:h0123456789abcdef
```

同名でもmodel/configが変わった個体が同じratingへ混ざることを防ぐ。

## Match scheduling

現在の `run_round_robin_league()` はdeterministic round-robin。

各pair/roundで色を交換する。

```text
A black vs B white
B black vs A white
```

各gameで両AI packageをreloadするためstateful memoryはgameごとにfresh。

実game seedは以下からdeterministicに導出する。

- league seed
- participant seed
- game index
- color

derived seedはgame logへ記録する。

## Rating

現在はonline Glicko-1 style。

- initial rating: 1500
- initial RD: 350
- RDをrating uncertaintyとして保持
- win/loss/draw countを別保持

full Glicko rating-period implementationではなくgameごとのonline update。

board sizeはparticipant identityに入るため6x6 / 8x8 / 10x10 ratingが偶然共有されない。

## Game log

game-log stream指定時、1 game / 1 JSON line:

```text
format = kadoka.league_game.v1
```

保存:

- deterministic `game_id`
- black/white participant ID
- board size
- actual derived seed
- result
- elapsed game time
- Headless AI-call / invalid-attempt metrics
- rating/RD before/after

Game Record v1のULIDとは別に、現league logは再現用deterministic game IDを持つ。Dataset Pool統合時はprovenance上の関連を明示する。

## Position dataset

position-dataset指定時:

```text
format = kadoka.league_position.v1
```

- `game_id`
- black/white participant
- board size
- canonical Headless state snapshot

弱いAI、character AI、old checkpoint、external AIをposition generatorとして利用できるが、そのmoveをtrusted labelとは扱わない。

強いrelabelはMulti-engine Relabeling側。

## Metrics

optional `HeadlessConfig::collect_metrics`:

- AI calls
- total/max `think()` time
- invalid attempt count
- per-turn invalid histogram
- game elapsed time

通常のhigh-throughput Dataset生成では明示しない限りper-call timing overheadを払わない。

## CLI

```text
kadoka_ai_league <board-size> <games-per-color> <seed> \
  <game-log.jsonl|-> <position-dataset.jsonl|-> \
  <manifest.json> <manifest.json> [...]
```

`-` はoutput無効。

## 現在scope

実装済み:

- reproducible participant identity
- round-robin
- mandatory color swap
- fresh package lifecycle per game
- online Glicko + RD
- 6x6 / 8x8 / 10x10
- common JSONL game log
- position Dataset output
- Headless timing / invalid metrics

Issue #1で継続:

- persistent league registry
- random / rating-band / Candidate / Hall-of-Fame scheduler
- explicit checkpoint registry
- search nodes/simulationsの共通metric
- 必要ならrating-period Glicko/Glicko-2
- Game Record / Dataset Pool provenanceとの正式統合
