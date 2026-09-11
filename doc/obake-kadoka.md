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

The game consumes only the selected move. AI Creator additionally receives raw candidate evaluation scores, post-randomizer policy values, and diagnostics.

## Character behavior

- legal-move input is discarded by `drop_legal_moves`
- Kadoka evaluates every empty square without knowing the legal-move list
- the evaluation function itself is intentionally sensible
- the evaluated candidates are converted to randomizer weights instead of always choosing the maximum
- the same board does not always produce the same move
- recent 1-2 attempted squares are remembered
- every attempt updates memory, including illegal attempts
- therefore repeated illegal attempts eventually overwrite older memories
- Kadoka may later retry a square it previously forgot

This memory is deliberately character-like and is not used as deep search.

## Obake-style evaluation

The square evaluator is separate from the randomizer. It scores every empty square using lightweight board features:

- corner preference
- edge preference
- X-square penalty while the related corner is empty
- C-square penalty while the related corner is empty
- occupied-neighbor density
- frontier-like penalty from surrounding empty cells
- bonus when both colors are present around the square
- line-potential bonus when an occupied run terminates in the opposite color
- mild center preference in early positions

The evaluator does not consume the legal-move list and does not call the Core legal-move generator. It may therefore assign a high score to an illegal square. The game remains responsible for rejecting illegal attempts and asking Kadoka again.

## Candidate randomizer

Raw evaluation score and move-selection probability are intentionally different values.

```text
empty squares
  -> evaluation score
  -> temperature-scaled exponential weight
  -> exploration floor
  -> recent-attempt penalty
  -> weighted random selection
```

AI Creator exposes:

- `candidate.value`: raw evaluation score
- `candidate.policy`: normalized post-randomizer probability

This makes the model inspectable while preserving its character behavior.

## Dataset-generation performance

Kadoka is implemented as a native C++ engine because character games may be used to generate many training positions.

The hot `think()` path:

- does not serialize JSON
- does not launch a process
- does not allocate a candidate vector
- uses a fixed `std::array` for up to 10x10 = 100 cells
- counts board phase information once per inference and reuses it for every candidate
- does not build diagnostics
- does not receive/copy legal moves after the adapter removes them

`inspect()` intentionally performs additional allocations because it is a development/analysis path and returns all candidate values/policies plus diagnostics.

For production self-play Dataset generation, use `think()` through the normal game/headless path.

## Model file

`model.json` uses:

```text
format = kadoka.native_model.v1
```

The engine implementation is separate from model parameters. This lets the same native engine load different parameter sets without changing the common AI protocol.
