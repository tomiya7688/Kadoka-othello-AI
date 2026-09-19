# Canonical Core API

Kadoka Othello AI uses one authoritative state contract at the Core boundary.

## State format

The canonical exchange format is JSON:

```text
format = kadoka.core_state.v1
```

A state contains only:

- board size and row-major cells
- side to move
- time information

Example:

```json
{
  "format": "kadoka.core_state.v1",
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

Cell values are:

- `0`: empty
- `1`: black
- `2`: white

Time values are milliseconds. `null` means that the current time-control policy does not provide that value.

## Not part of Core state

The canonical state never contains:

- legal move lists
- candidate lists or rankings
- evaluations
- mobility/openness features
- search diagnostics
- training labels
- game history
- terminal result metadata

These are derived by an AI, AI Runtime helper, AI Creator or Dataset/Analysis tooling.

## Native fast path

JSON is the public semantic source of truth, but native Runtime code does not serialize and parse JSON on every move.

`CoreStateView` is the typed zero-copy view corresponding one-to-one with `kadoka.core_state.v1`:

```text
Board reference + side_to_move + time
```

Native AI engines receive that view directly. If an AI needs legal moves it derives them from the board and side to move.

`core_state_to_json()` and `parse_core_state_json()` define and test the exchange representation. Internal bitboards, fixed buffers, tensors or other compressed forms remain implementation details.

## AI output and legality

An AI returns a proposed move. The proposal is never authoritative.

`Game::play()` performs the authoritative legality check and state transition.

For an illegal move:

- board is unchanged
- side to move is unchanged
- ply is unchanged
- normal legal-move history is unchanged
- a `GameEventType::InvalidMove` event is emitted

Headless may ask the same AI again on the same state subject to its retry limit.

## Game events

The Core event contract currently exposes:

- `MoveAccepted`
- `InvalidMove`
- `Pass`
- `Terminal`

Listeners are optional. The normal state-transition path does not require a logger or GUI.

The event contract is the basis for the separate Game Record v1 work in Issue #18.

## External/script AI

Persistent external sessions keep their framing for request IDs and process lifetime, but the state payload itself is canonical JSON:

```text
request 1
state {"format":"kadoka.core_state.v1",...}
end
```

No legal-move records are sent.

## Training formats

Binary, compressed, tensor and feature-rich training representations are not alternative Core APIs.

They belong to AI Creator / Dataset Tooling and must be derivable from canonical state plus explicitly separate provenance/analysis records.
