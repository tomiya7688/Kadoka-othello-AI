# State / Dataset Format

## Cell values

- `0`: Empty
- `1`: Black
- `2`: White

## Canonical Core State JSON

JSON is the canonical format for state exchange across the Othello Core boundary.

The canonical state contains only:

- board information
- side to move
- time information

Example:

```json
{
  "board_size": 8,
  "cells": [0, 0, 0, 0],
  "current_player": "black",
  "time": {
    "black_ms": 300000,
    "white_ms": 300000
  }
}
```

The exact time-control fields may be extended while preserving the rule that Core state exchange carries game state/time, not AI-analysis annotations.

The canonical Core state does **not** include:

- legal moves
- evaluation values
- mobility / openness analysis
- candidate rankings
- learning labels
- compressed training representations

These are derived or tooling-owned data.

## Move and invalid-move event

A move is submitted separately from the state.

The Core validates it against the authoritative rules.

If the move is illegal:

- board state is unchanged;
- side to move is unchanged;
- an invalid-move event is emitted.

The event can be recorded by GUI, Headless, character behavior or developer tooling without being written as a successful move in ordinary game history.

## Dataset records

AI Creator / Dataset Tooling may enrich canonical states with fields such as:

- legal moves
- successful move history
- game result
- evaluations
- search diagnostics
- model/version provenance
- relabeling metadata

JSON Lines is the default interoperable/debug-friendly dataset representation.

A dataset record may therefore contain more information than the canonical Core State JSON. That does not make those extra fields part of the Core API.

## Compression and training formats

Fixed-length binary, compressed formats, bitboards, tensors and other high-throughput training representations are AI Creator / Dataset Tooling concerns.

They may be generated from canonical JSON / JSONL and converted back when needed.

The Core API remains JSON regardless of dataset storage optimizations.


## End-of-game record export

対局終了時の履歴出力は、Kadoka Shougi AI Issue #85 の方針をOthello向けに採用し、盤面系列と補助イベント系列を分離する。

### Output 1: BoardState JSONL

1行1局面のJSON Lines形式。

各recordは、そのply時点の正規Core stateを保存する。

必須項目:

- `schema`: `kadoka.othello.board_state`
- `version`: 1
- `game_id`: 対局を一意に識別するULID
- `ply`: 成功した合法着手数。初期局面は0
- `board_size`
- `cells`
- `side_to_move`
- `time`

例:

```json
{
  "schema": "kadoka.othello.board_state",
  "version": 1,
  "game_id": "01J...",
  "ply": 0,
  "board_size": 8,
  "cells": [0, 0, 0, 0],
  "side_to_move": "black",
  "time": {
    "black_ms": 300000,
    "white_ms": 300000,
    "per_move_limit_ms": null
  }
}
```

### Output 2: GameAux JSONL

盤面そのものは持たず、実際に発生した操作・イベント・終局情報を保存する。

必須項目:

- `schema`: `kadoka.othello.game_aux`
- `version`: 1
- `game_id`
- `ply`
- `event_index`
- `event_type`
- `actor`
- `action`
- `result`
- `terminal`

合法着手イベント例:

```json
{
  "schema": "kadoka.othello.game_aux",
  "version": 1,
  "game_id": "01J...",
  "ply": 1,
  "event_index": 1,
  "event_type": "action",
  "actor": "black",
  "action": {
    "type": "move",
    "row": 2,
    "col": 3
  },
  "result": {
    "status": "accepted",
    "reason": null
  },
  "terminal": null
}
```

違法手イベント例:

```json
{
  "schema": "kadoka.othello.game_aux",
  "version": 1,
  "game_id": "01J...",
  "ply": 0,
  "event_index": 1,
  "event_type": "action",
  "actor": "black",
  "action": {
    "type": "move",
    "row": 0,
    "col": 0
  },
  "result": {
    "status": "illegal",
    "reason": "illegal_move"
  },
  "terminal": null
}
```

終局イベント例:

```json
{
  "schema": "kadoka.othello.game_aux",
  "version": 1,
  "game_id": "01J...",
  "ply": 60,
  "event_index": 61,
  "event_type": "terminal",
  "actor": null,
  "action": null,
  "result": null,
  "terminal": {
    "result": "black_win",
    "reason": "normal",
    "black_discs": 38,
    "white_discs": 26
  }
}
```

### ID / index rules

- `game_id`: ULID文字列
- `ply`: 成功した合法着手数
- 初期局面は `ply = 0`
- 違法手では `ply` を増やさない
- passでも `ply` を増やさない
- `event_index`: 実際に発生したイベント通番
- 同一 `game_id + ply` で盤面系列と補助情報をjoin可能
- 同一plyで複数回違法手があっても `event_index` で一意化

### Pass

パスはOthello固有イベントとしてGameAuxへ保存する。

`ply` は成功した合法着手数だけを数えるため、passでは増加しない。

Coreがパスを自動処理する場合でも、履歴上は明示的なeventとして残す。
pass後に手番だけが変化した状態をBoardStateとして保存する必要がある場合は、同一`ply`に対する状態遷移として扱い、`event_index`で時系列を区別する。

### Separation rule

BoardStateは盤面系列・画像再生成・盤面学習ground truth向け。

GameAuxは以下のような補助履歴向け:

- accepted / illegal action
- invalid-move event
- pass
- time-related event
- terminal result
- final disc counts

AI探索ログ、候補手評価、合法手一覧、解放度、relabel、rating等はこの2つのCore対局履歴へ混ぜず、AI Creator / Dataset / Analysis層で別途保持する。

### Storage

- UTF-8
- JSON Lines (`.jsonl`)
- 1行1record
- `schema` / `version` 必須
- unknown optional fieldは無視可能
- breaking changeのみversion更新

この対局履歴JSONLが学習用圧縮形式そのものになる必要はない。
AI Creator / Dataset Toolingが必要に応じて圧縮・binary・tensor等へ変換する。
