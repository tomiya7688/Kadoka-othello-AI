# Kadoka Othello AI

C++ Othello core and AI development project.

Current core supports 6x6 / 8x8 / 10x10 through the same variable-size board implementation.

## Build on Windows

```bat
build.bat
```

The script configures CMake, builds Release, and runs tests.

## Headless random games

```bat
build\Release\kadoka_othello_headless.exe 10000 8 dataset.jsonl 12345
```

Arguments are `games board_size output.jsonl seed`.

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

## Licensing

- Software, build scripts, and ordinary documentation: MIT License (`LICENSE`)
- Kadoka (かどか) and Maru (まる) character materials: Obake Character License v1.1 (`CHARACTER_LICENSE.md`)
- Other named AI-model characters, their settings, identity, and dedicated character assets: Kadoka AI Character License v1.0 (`AI_CHARACTER_LICENSE.md`)
- A model's algorithm, trained weights, or dataset may have an additional individual license when explicitly specified.

## Documents

- `doc/architecture.md` - responsibility boundaries and source layout
- `doc/current-state.md` - compact current implementation state
- `doc/context-routing.md` - context and validation routing map
- `doc/coding-rules.md` - coding rules, dependency rules and performance exceptions
- `doc/runtime-creator-boundary.md` - AI Runtime / AI Creator boundary
- `doc/model-format.md` - root model descriptor and asset structure
- `doc/script-evaluator-runtime.md` - evaluator runtimes and native in-process ABI
- `doc/build.md` - build and runner usage
- `doc/data-format.md` - GameSnapshot / JSON Lines format

All C++ source and headers are kept under `src/`.
