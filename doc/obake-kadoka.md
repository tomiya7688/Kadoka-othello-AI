# Obake Kadoka AI

## Package

Obake Kadoka is the reference Kadoka-native distributable model package.

```text
src/packages/obake_kadoka/
  manifest.json
  model.json
```

It follows the same AI Creator package format used by other Kadoka models.

## Creator usage

```bat
build\Release\kadoka_othello_ai_creator.exe analyze ^
  src\packages\obake_kadoka\manifest.json ^
  samples\position_8x8.txt ^
  obake_kadoka.jsonl
```

The game consumes only the selected move. AI Creator additionally receives candidate weights/policies and diagnostics.

## Character behavior

- legal-move input is discarded by `drop_legal_moves`
- Kadoka selects among empty squares using a lightweight weighted randomizer
- the same board does not always produce the same move
- recent 1-2 attempted squares are remembered
- every attempt updates memory, including illegal attempts
- therefore repeated illegal attempts eventually overwrite older memories
- Kadoka may later retry a square it previously forgot

This memory is deliberately character-like and is not used as deep search.

## Current weighting

The model config contains lightweight parameters for:

- corner preference
- edge preference
- adjacency to existing stones
- simple directional-pattern preference
- mild center preference
- recent-attempt penalty
- random exploration floor

These are not intended to make Kadoka a strong Othello engine. They make it somewhat sensible while preserving the character behavior and illegal-move possibility.

## Dataset-generation performance

Kadoka is implemented as a native C++ engine because character games may be used to generate many training positions.

The hot `think()` path:

- does not serialize JSON
- does not launch a process
- does not allocate a candidate vector
- uses a fixed `std::array` for up to 10x10 = 100 cells
- does not build diagnostics
- does not receive/copy legal moves after the adapter removes them

`inspect()` intentionally performs additional allocations because it is a development/analysis path and returns all candidate weights plus diagnostics.

For production self-play Dataset generation, use `think()` through the normal game/headless path.

## Model file

`model.json` uses:

```text
format = kadoka.native_model.v1
```

The engine implementation is separate from model parameters. This lets the same native engine load different parameter sets without changing the common AI protocol.
