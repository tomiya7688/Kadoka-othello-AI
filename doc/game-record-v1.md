# Game Record v1

Game Record v1は実際のOthello対局を2本のUTF-8 JSON Linesへ分離保存する。

- **BoardState JSONL**: canonical position sequence
- **GameAux JSONL**: action / illegal attempt / pass / terminal等

`game_id + ply` でjoinし、同一plyの複数eventは `event_index` で順序付けする。

## BoardState JSONL

```text
schema = kadoka.board_state
version = 1
```

例:

```json
{
  "schema": "kadoka.board_state",
  "version": 1,
  "game_id": "01ARZ3NDEKTSV4RRFFQ69G5FAV",
  "ply": 0,
  "board_size": 8,
  "cells": [0, 0, 0, 0],
  "side_to_move": "black",
  "time": {
    "black_remaining_ms": null,
    "white_remaining_ms": null,
    "move_limit_ms": null
  }
}
```

board fieldの意味は `kadoka.core_state.v1` と同じ。

書くタイミング:

- initial position: `ply=0`
- accepted move後
- accepted pass後

illegal proposalでは新BoardStateを書かない。

legal moves / evaluation / search diagnostics / rating / training labelは含めない。

## GameAux JSONL

```text
schema = kadoka.game_aux
version = 1
```

accepted move例:

```json
{
  "schema": "kadoka.game_aux",
  "version": 1,
  "game_id": "01ARZ3NDEKTSV4RRFFQ69G5FAV",
  "ply": 1,
  "event_index": 0,
  "event_type": "accepted_move",
  "actor": "black",
  "action": {"type": "move", "row": 2, "col": 3},
  "result": {"status": "accepted", "reason": null},
  "terminal": null
}
```

illegal proposalは同じplyで2 Aux recordを生成する。

```text
illegal_move
invalid_move_notification
```

1つ目はrejectされたproposal、2つ目はGUI/Headless/character/loggerが購読できるCore notification。

いずれもBoardStateを追加しない。

### Pass

```json
{
  "event_type": "pass",
  "action": {"type": "pass"},
  "result": {"status": "accepted", "reason": null}
}
```

passは1手番を正常消化するため `ply + 1`。

対応BoardStateはcells不変で `side_to_move` が次手番へ変わる。

### Terminal

```json
{
  "event_type": "terminal",
  "actor": null,
  "action": null,
  "result": null,
  "terminal": {
    "winner": "black",
    "black_discs": 35,
    "white_discs": 29
  }
}
```

## Index rule

- `game_id`: 26文字ULID
- initial BoardState: `ply=0`
- accepted move: `ply + 1`
- pass: `ply + 1`
- illegal: ply不変
- `event_index`: 0開始、GameAux recordごとに+1
- 全GameAuxは同じ `game_id + ply` のBoardStateへjoin可能
- terminalはfinal positionのplyを共有

## Runtime architecture

`Game` はauthoritative state transitionとlightweight typed eventだけを所有する。

`GameRecordRecorder` がeventを購読してRecordを構築する。

GameはJSON writerへ依存しない。

Recorder未接続の通常playではRecord serialization costを払わない。

Headlessはgame terminal後に2 streamを書き出す。

## Reader / Writer

- `board_state_record_to_json()`
- `game_aux_record_to_json()`
- 各record parser
- `write_game_record_jsonl()`
- `read_game_record_jsonl()`
- `read_game_records_jsonl()`

readerはv1 schema/versionとrequired fieldを検証し、unknown optional fieldを無視できる。

## Headless CLI

```text
kadoka_othello_headless \
  <games> <board-size> <legacy-state-output|-> <seed> \
  <black-manifest|-> <white-manifest|-> \
  <board-state.jsonl|-> <game-aux.jsonl|->
```

Record pathは2本同時指定する。

例:

```text
kadoka_othello_headless 100 8 - 12345 - - board-state.jsonl game-aux.jsonl
```

legacy state outputはcompatibilityのため当面残す。

Dataset Poolはtransitional streamを拡張せずRecord/provenanceを利用する。

## 非対象

- compressed/tensor training format
- AI search log
- legal move list
- evaluation
- rating
- relabel
- GUI presentation

これらはAI Creator / Dataset / Analysis責務。
