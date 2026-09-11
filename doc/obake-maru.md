# Obake Maru AI

## Package

```text
src/packages/obake_maru/
  manifest.json
  model.json
```

Maru uses the same AI Creator native package format as Obake Kadoka.

## Character behavior

- legal-move input is discarded by `drop_legal_moves`
- Maru does not know legal moves
- Maru evaluates all empty squares with a very small local heuristic
- Maru strongly randomizes among candidates
- Maru remembers only the immediately previous placement attempt
- if the same board is shown again, Maru infers that the previous attempt was rejected
- that single rejected square is strongly discouraged on the next attempt
- after Maru tries another square, the older mistake is forgotten

This matches the intended behavior: after being told a square is bad, Maru can effectively react as if saying `ここだめなのだ？` and try somewhere else, but it does not retain older mistakes.

## Evaluation style

Maru's evaluator is deliberately much simpler than Kadoka's.

It prefers:

- squares close to existing stones
- locally dense / busy-looking areas
- squares touching both colors a little
- a mild center tendency

It does not inspect bracket structures as deeply as Kadoka and uses a high randomizer temperature plus a large exploration floor. The intended result is visibly non-uniform play that is still weak and clumsy.

## Strength target

Maru should usually be weaker than Kadoka and only modestly stronger than uniform random after illegal attempts are filtered by the game. Exact strength must be measured by league games.

## Creator usage

```bat
build\Release\kadoka_othello_ai_creator.exe analyze ^
  src\packages\obake_maru\manifest.json ^
  samples\position_8x8.txt ^
  obake_maru.jsonl
```
