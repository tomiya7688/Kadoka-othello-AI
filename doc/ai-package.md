# AI Package Specification

## Goal

Kadoka Othello AI treats built-in and external AIs as packages behind one common protocol.

Game input is always:

- current board
- legal moves

Game output is always:

- selected move

Development tools may additionally request inspection data.

## manifest.json

Example:

```json
{
  "id": "kadoka.random",
  "name": "Kadoka Random AI",
  "version": "0.1.0",
  "protocol_version": 1,
  "board_sizes": [6, 8, 10],
  "interface": "native",
  "adapter": "pass_through",
  "entry": "builtin:kadoka.random",
  "capabilities": ["move", "inspection"]
}
```

## Interface types

- `native`
- `dynamic_library`
- `external_process`
- `python`
- `network`

The initial runtime loader implements built-in `native` packages. Other interface types are recognized and reserved for later loaders.

## Adapter

Initial adapters:

- `pass_through`: board + legal moves
- `drop_legal_moves`: board only

`drop_legal_moves` is intended for models such as Obake Kadoka / Obake Maru that should not know legal moves.

## Distribution

A package may later contain:

```text
my_ai/
  manifest.json
  model.bin
  engine.dll
  config/
```

The manifest is the stable package entry point. Runtime implementation details may vary without changing the game protocol.
