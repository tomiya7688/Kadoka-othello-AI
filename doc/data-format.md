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
