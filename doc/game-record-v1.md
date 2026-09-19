# Game Record v1

Game Record v1 stores an actual Othello game as two independent UTF-8 JSON Lines streams.

The split is intentional:

- **BoardState JSONL** is the canonical position sequence.
- **GameAux JSONL** stores actions, invalid attempts, pass and terminal information without duplicating the board.

The streams join on `game_id + ply`. Multiple events on the same ply are ordered by `event_index`.

## BoardState JSONL

Schema:

```text
schema = kadoka.board_state
version = 1
```

Example:

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

The board fields have the same meaning as `kadoka.core_state.v1`.

BoardState is written:

- once for the initial position at `ply = 0`
- after every accepted move
- after every accepted pass

It is **not** written for an illegal proposal.

It never contains legal moves, evaluation, search diagnostics, rating or training labels.

## GameAux JSONL

Schema:

```text
schema = kadoka.game_aux
version = 1
```

Accepted move:

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

Illegal proposal generates two auxiliary records at the unchanged ply:

```text
illegal_move
invalid_move_notification
```

The first records the rejected proposal. The second records the Core notification consumed by GUI / Headless / character behavior / loggers.

No BoardState is added for either record.

Pass:

```json
{
  "event_type": "pass",
  "action": {"type": "pass"},
  "result": {"status": "accepted", "reason": null}
}
```

A pass consumes a turn, so `ply` increases by one. Its BoardState has unchanged cells and the next `side_to_move`.

Terminal event:

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

## Index rules

- `game_id` is a 26-character ULID.
- initial BoardState is `ply = 0`
- accepted move increments `ply`
- pass increments `ply`
- illegal proposal does not increment `ply`
- `event_index` starts at zero and increases for every GameAux row
- every GameAux row can be joined to the BoardState with the same `game_id + ply`

A terminal event shares the final position's ply.

## Runtime architecture

`Game` owns only authoritative state transitions and emits lightweight typed events.

`GameRecordRecorder` subscribes to those events and accumulates Record v1 data. The game does not depend on JSON writer logic.

Normal play pays no Record serialization cost unless a recorder is explicitly attached.

Headless writes the two streams only after each game reaches terminal state.

## Reader / writer

Runtime exposes:

- `board_state_record_to_json()`
- `game_aux_record_to_json()`
- parsers for both record types
- `write_game_record_jsonl()`
- `read_game_record_jsonl()`
- `read_game_records_jsonl()`

Readers ignore unknown optional fields while requiring the v1 schema/version and known required fields.

## Headless CLI

Existing arguments remain compatible.

Optional Record paths are appended:

```text
kadoka_othello_headless \
  <games> <board-size> <legacy-state-output|-> <seed> \
  <black-manifest|-> <white-manifest|-> \
  <board-state.jsonl|-> <game-aux.jsonl|->
```

Both Record paths must be supplied together.

Example:

```text
kadoka_othello_headless 100 8 - 12345 - - board-state.jsonl game-aux.jsonl
```

The legacy state output remains temporarily available for compatibility. Dataset Pool work should consume Record/provenance rather than extending that transitional stream.

## Scope

Record v1 deliberately excludes:

- compressed/tensor training formats
- AI search logs
- legal move lists
- evaluations
- rating
- relabel data
- GUI presentation

Those belong to AI Creator / Dataset / Analysis layers.
