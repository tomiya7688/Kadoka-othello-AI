# Obake Kadoka AI

## Package

Obake Kadoka is the reference Kadoka-native distributable model package.

```text
src/packages/obake_kadoka/
  manifest.json
  model.json
```

It follows the same AI Creator package format used by other Kadoka models.

## Character behavior

- legal-move input is discarded by `drop_legal_moves`
- Kadoka evaluates every empty square without asking whether it is legal
- the evaluator is coherent, but it is not a normal Othello engine evaluator
- candidate selection is randomized from the evaluator scores
- the same board does not always produce the same move
- Kadoka remembers only the most recent 1-2 placement attempts
- memory stores the attempted square and the board hash seen before the attempt
- if Kadoka is called again with the same board hash, it infers that the previous attempt was rejected
- rejected recent squares receive a very strong retry penalty
- every new attempt overwrites short-term memory, including illegal attempts
- forgotten attempts may later be retried

## Obake-style evaluator

The evaluator does not use the supplied legal move list and does not deliberately implement normal strategic Othello knowledge such as opening books, corner/X/C tables, parity, mobility search, or exact legal-move filtering.

Instead it judges whether an empty square *looks like a meaningful place to put a stone* from the visible local stone pattern.

Current signals include:

- number of occupied neighboring squares
- penalty for isolated/open surroundings
- whether both black and white stones touch the square
- local color transitions along rays
- line/bracket interest: a same-color run terminating in the opposite color
- local density
- a small early-game center tendency

The line/bracket signal is deliberately the strongest feature. This means Kadoka often prefers squares that would interact with or flip a meaningful run of stones for one of the two colors, even though Kadoka does not know whether that square is legal for its own side. If the game rejects the attempt, short-term memory makes an immediate retry unlikely.

This produces the intended character: the placement-evaluation function itself is sensible, but Kadoka does not understand the legal-move set.

## Expected play style

Kadoka should be clearly stronger than uniform random play because it strongly prefers locally active squares and bracket-like structures instead of arbitrary empty cells. It is intentionally not a strategic engine and should not be expected to play like a dan-level or search-based AI.

The target character strength is roughly an average casual player / beginner-to-intermediate feel after illegal retries are filtered by the game. Actual strength must be measured by league games rather than assumed from the heuristic alone.

## Dataset-generation performance

Kadoka is implemented as a native C++ engine because character games may be used to generate many training positions.

The hot `think()` path:

- does not serialize JSON
- does not launch a process
- does not allocate a candidate vector
- uses a fixed `std::array` for up to 10x10 = 100 cells
- does not build diagnostics
- does not receive/copy legal moves after the adapter removes them
- computes the board empty count only once per inference
- keeps only a two-entry fixed-size attempt history

`inspect()` intentionally performs additional allocations because it is a development/analysis path and returns all candidate values/policies plus diagnostics.

For production self-play Dataset generation, use `think()` through the normal game/headless path.
