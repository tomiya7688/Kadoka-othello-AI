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

## Documents

- `doc/architecture.md` - responsibility boundaries and source layout
- `doc/build.md` - build and runner usage
- `doc/data-format.md` - GameSnapshot / JSON Lines format

All C++ source and headers are kept under `src/`.
