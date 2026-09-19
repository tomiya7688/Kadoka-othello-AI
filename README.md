# Kadoka Othello AI

C++ Othello core and AI development project.

Current core supports 6x6 / 8x8 / 10x10 through the same variable-size board implementation.

## Build on Windows

```bat
build.bat
```

The script runs the project rule checker through CMake, builds Release, and runs tests.

## CI

The sibling-project CI baseline is enabled:

- Linux: CMake build + CTest + fixed-seed headless smoke
- Windows: `build.bat` + fixed-seed headless smoke + developer build artifact upload

The Windows artifact is currently a developer build, not yet a formally defined portable distribution package.

## Headless random games

```bat
build\Release\kadoka_othello_headless.exe 10000 8 dataset.jsonl 12345
```

Arguments start with `games board_size legacy_state_output seed`.

To emit the canonical two-stream Game Record v1 without the transitional state output:

```bat
build\Release\kadoka_othello_headless.exe 100 8 - 12345 - - board-state.jsonl game-aux.jsonl
```

## AI-assisted development

Start from `AI_CONTEXT.md` instead of preloading the whole repository.

Useful compact-routing commands:

```text
python tools/context_route.py --list
python tools/context_route.py script-evaluator
python tools/next_issue.py
```

- `AI_CONTEXT.md` - small AI entrypoint and invariants
- `doc/current-state.md` - compact implementation-status index
- `doc/context-routing.md` - task/change -> source/tests/docs/validation routes
- `tools/next_issue.py` - compact Goal / Required / Acceptance task capsule

These are indexes only. Source, tests, Issues and detailed specs remain the source of truth.

## Coding rules

This project adopts the applicable parts of `tomiya7688/upd-commander-base-design`.
See `doc/coding-rules.md` for project-specific rules and Runtime performance exceptions.

C++ formatting/static-analysis baselines are shared with Kadoka Shougi AI through `.clang-format` and `.clang-tidy`.

## Sibling projects

Kadoka Shougi AI and Kadoka Tetris AI are sibling projects. Proven CI, validation, context-routing and architecture techniques should be cross-adopted when they solve the same problem without harming game-specific semantics or hot-path performance.

See `doc/sibling-project-alignment.md`.

## Licensing

- Software, build scripts, and ordinary documentation: MIT License (`LICENSE`)
- Kadoka (かどか) and Maru (まる) character materials: Obake Character License v1.1 (`CHARACTER_LICENSE.md`)
- Other named AI-model characters, their settings, identity, and dedicated character assets: Kadoka AI Character License v1.0 (`AI_CHARACTER_LICENSE.md`)
- A model's algorithm, trained weights, or dataset may have an additional individual license when explicitly specified.

## Documents

- `doc/architecture.md` - responsibility boundaries and source layout
- `doc/core-api.md` - canonical JSON Core state and invalid-move event contract
- `doc/current-state.md` - compact current implementation state
- `doc/context-routing.md` - context and validation routing map
- `doc/sibling-project-alignment.md` - cross-project engineering reuse policy
- `doc/coding-rules.md` - coding rules, dependency rules and performance exceptions
- `doc/runtime-creator-boundary.md` - AI Runtime / AI Creator boundary
- `doc/model-format.md` - root model descriptor and asset structure
- `doc/script-evaluator-runtime.md` - evaluator runtimes and native in-process ABI
- `doc/build.md` - build and runner usage
- `doc/data-format.md` - canonical state / Dataset format boundaries
- `doc/game-record-v1.md` - BoardState + GameAux JSONL game history

All C++ source and headers are kept under `src/`.
