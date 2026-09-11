# AI Creator Tool

## Current scope

`kadoka_othello_ai_creator` is an initial mock/development CLI for validating AI packages and inspection output.

It currently:

1. loads `manifest.json`
2. creates a supported AI package
3. creates an initial Othello position
4. sends board + legal moves through the package adapter
5. requests `inspect()` output
6. prints diagnostics
7. writes one `ModelRecord` using `kadoka.jsonl.v1`

Example:

```bat
build\Release\kadoka_othello_ai_creator.exe src\packages\random\manifest.json sample.jsonl 8
```

## Separation from game runtime

Game runtime uses `think()` and consumes only the selected move.

AI development tools use `inspect()` and may receive:

- selected move
- candidate evaluations
- policy values
- value estimates
- search statistics
- confidence / uncertainty
- arbitrary diagnostics

This keeps the game protocol minimal while allowing rich training and debugging output.

## Planned extensions

- position/file input
- repeated inference benchmark
- dataset conversion
- model comparison
- candidate visualization export
- adapter testing
- dynamic library package loading
- external process / Python package loading
- codec selection from CLI
