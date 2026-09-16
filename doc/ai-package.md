# AI Package Specification

## Goal

Kadoka Othello AI treats built-in and external AIs as packages behind one common protocol.

Game input is always:

- current board
- legal moves

Game output is always:

- selected move

Development tools may additionally request inspection data.

## AI Creator standard package

AI Creator uses the following package layout as the standard form for Kadoka models and imported AIs.

```text
my_ai/
  manifest.json
  metadata.json
  model.json | model.bin | model-specific files
  optional runtime files
  optional assets/
```

`manifest.json` is always the package entry point. `model` is optional for engines that do not require separate model data. `metadata` is optional for legacy/internal packages but recommended for new distributable models.

Example:

```json
{
  "id": "kadoka.obake_kadoka",
  "name": "Obake Kadoka",
  "version": "0.1.0",
  "protocol_version": 1,
  "board_sizes": [6, 8, 10],
  "interface": "native",
  "adapter": "drop_legal_moves",
  "entry": "builtin:kadoka.obake_kadoka",
  "model": "model.json",
  "metadata": "metadata.json",
  "capabilities": ["move", "inspection", "dataset_generation"]
}
```

## Required manifest fields

- `id`
- `name`
- `version`
- `protocol_version`
- `board_sizes`
- `interface`
- `adapter`
- `entry`

Optional:

- `model`
- `metadata`
- `capabilities`

## Model files

`model.json` is the Othello model root descriptor and may reference evaluator, behavior, network, opening, search, scripted evaluator, or other assets. Large learned models may use binary weights while keeping the same package manifest contract.

The execution and metadata responsibilities are intentionally separate:

```text
manifest -> engine selection / runtime entry
manifest.model -> executable model/config/assets
manifest.metadata -> Kadoka AI family identity/provenance/requirements/benchmark metadata
```

This lets model packages be updated or distributed without changing the game protocol while still exposing common metadata to sibling-project tooling.

`metadata.json` uses `kadoka.ai_metadata.v1`; see `doc/family-model-metadata.md`.

## Interface types

- `native`: in-process C++ implementation, fastest path for built-in/Kadoka models
- `dynamic_library`: reserved for separately distributed native engines
- `external_process`: executable process adapter
- `python`: Python process adapter
- `network`: reserved for remote inference

For high-throughput Dataset generation, prefer `native` or later `dynamic_library`; process-based adapters are compatibility paths rather than the fastest path.

## Adapter

Initial adapters:

- `pass_through`: board + legal moves
- `drop_legal_moves`: board only

`drop_legal_moves` is used by Obake Kadoka / Obake Maru. The game still provides legal moves to the package protocol, but the adapter removes them before the character AI sees the input.

## Standard example: Obake Kadoka

```text
src/packages/obake_kadoka/
  manifest.json
  metadata.json
  model.json
  evaluator.json
  behavior.json
```

Obake Kadoka is intentionally implemented as a native model so Dataset generation can call it without process startup, JSON serialization, or temporary-file overhead.
