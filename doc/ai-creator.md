# AI Creator Tool

## Purpose

`kadoka_othello_ai_creator` is the development CLI for validating, comparing, benchmarking, and inspecting AI packages without involving the GUI.

The game runtime consumes only the selected move from `think()`.
The AI Creator uses `inspect()` and may receive richer development output such as candidate evaluations, policy, value, timing, and diagnostics.

## Commands

### Analyze one AI

```bat
build\Release\kadoka_othello_ai_creator.exe analyze src\packages\random\manifest.json
```

With an arbitrary position and output file:

```bat
build\Release\kadoka_othello_ai_creator.exe analyze src\packages\random\manifest.json samples\position_8x8.txt result.jsonl
```

The command prints:

- selected move
- inference time
- candidate moves
- candidate value
- candidate policy
- diagnostics

It also writes a `kadoka.jsonl.v1` ModelRecord.

### Compare multiple AIs on the same position

```bat
build\Release\kadoka_othello_ai_creator.exe compare samples\position_8x8.txt manifest_a.json manifest_b.json
```

Each AI receives exactly the same board and legal-move list through its configured adapter.
This is intended for model comparison, regression checks, and disagreement dataset generation.

### Benchmark inference

```bat
build\Release\kadoka_othello_ai_creator.exe benchmark src\packages\random\manifest.json 10000 samples\position_8x8.txt
```

Outputs:

- iterations
- total time in microseconds
- average inference time
- minimum inference time
- maximum inference time

This path is intended for Hyper Faster and other latency-sensitive AI development.

### List known data mappings

```bat
build\Release\kadoka_othello_ai_creator.exe formats
```

This prints the static field mapping tables used for Kadoka ModelRecord interoperability.

## Position file format

The current lightweight text input format is intentionally simple and static.

First line:

```text
<board_size> <black|white> <ply>
```

Then exactly `board_size` rows follow.

Characters:

- `B` = black
- `W` = white
- `.` = empty

Example:

```text
8 black 0
........
........
........
...WB...
...BW...
........
........
........
```

The AI Creator recalculates legal moves from the supplied board and player using the Core rules engine.

## Inspection contract

`AIInspection` currently contains:

- selected move
- candidate list
  - move
  - value
  - policy
- arbitrary diagnostics

An AI does not need to provide all candidate information. Missing information may remain empty or zero depending on the AI type.

The Random AI uses uniform policy over legal moves as a minimal working example.

## Separation from game runtime

Game runtime:

```text
board + legal moves -> adapter -> AI think() -> selected move
```

AI Creator:

```text
board + legal moves -> adapter -> AI inspect()
                              -> selected move
                              -> candidates
                              -> diagnostics
                              -> benchmark data
                              -> ModelRecord
```

This keeps the game path minimal while letting development tools receive richer outputs.

## Planned next extensions

- selectable output codec
- static conversion execution, not only mapping display
- candidate visualization export for GUI
- batch position analysis
- disagreement extraction
- dynamic-library package loading
- external-process and Python package loading
- per-stage timing diagnostics
